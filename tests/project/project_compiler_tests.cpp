/**
 * \file tests/project/project_compiler_tests.cpp
 *
 * packaging a project: what is checked before anything is written, what ends up in the
 * package, and what a failed compile leaves behind. the runtime, the engine's assemblies and
 * its lua files are stand-ins in a temp folder; the engine's resources are the real ones.
 * then what the editor's window is built on: a preview of a compile, the choices written
 * back to the project file, and a compile that runs on a worker thread.
 * last, what a package leaves behind: scenes go in as binary, and a file of a kind a scene
 * reaches is left out when nothing reaches it.
 **/
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#include <asio/asio.hpp>

#include "core/config_table.hpp"
#include "core/job_system.hpp"
#include "event/event_system.hpp"
#include "file/filesystem.hpp"
#include "file/path_helpers.hpp"
#include "serialization/scene_serializer.hpp"

#include "project/compile_session.hpp"
#include "project/pipeline_collector.hpp"
#include "project/project_compiler.hpp"
#include "vm/builtin_command_devices.hpp"
#include "vm/command_bus.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    constexpr std::string_view kProject =
      "# a comment that has to survive the packaging\n"
      "[project]\n"
      "metadata = [ { key = \"name\", value = \"Sandbox Game\" } ]\n"
      "\n"
      "[scripting]\n"
      "projectrc-path = \"game/project.lua\"\n"
      "\n"
      "[scene-graph]\n"
      "starting-scene = \"Main\"\n"
      "scenes = [\n"
      "  { name = \"Main\", path = \"game/assets/scenes/main.oscn\" },\n"
      "  { name = \"Second\", path = \"game/assets/scenes/second.oscn\" },\n"
      "]\n";

    constexpr std::string_view kScene = "[scene]\nschema-version = 1\nname = \"main\"\n";

    /// a scene that uses a model, a material and a sound of the project
    constexpr std::string_view kSceneWithContent =
      "[scene]\n"
      "schema-version = 1\n"
      "name = \"main\"\n"
      "\n"
      "[[objects]]\n"
      "id = 1\n"
      "name = \"Crate\"\n"
      "[objects.components.render]\n"
      "model_asset_id = \"game/assets/models/crate.obj\"\n"
      "material_asset_id = \"game/assets/materials/crate.omat\"\n"
      "[objects.components.audio-source]\n"
      "clip_asset_id = \"game/assets/audio/thud.wav\"\n";

    /// one triangle, with a material file that names a texture beside the model
    constexpr std::string_view kCrateModel = "mtllib crate.mtl\nusemtl skin\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n";
    constexpr std::string_view kCrateModelMaterial = "newmtl skin\nKd 1 1 1\nmap_Kd crate-skin.png\n";
    constexpr std::string_view kCrateMaterial =
      "asset-type = \"material\"\n"
      "name = \"crate\"\n"
      "\n"
      "[params]\n"
      "roughness = 0.5\n"
      "\n"
      "[textures]\n"
      "base_color = \"../textures/paint.png\"\n";

  }  // namespace

  class project_compiler_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-project-compiler-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      project_folder = sandbox / "game";
      output = sandbox / "out";

      /// a compile mounts the project and the engine, and a mount needs somewhere to send its
      ///  change events; a driver sets that up, and so does the command-line runtime
      events = std::make_unique<event_system>(io);
      subsystem<file_system>::get()->initialize_file_events(*events);

      write("game/game.toml", kProject);
      write("game/project.lua", "-- project settings\n");
      write("game/assets/scenes/main.oscn", kScene);
      write("game/assets/scenes/second.oscn", kScene);
      write("game/assets/textures/wall.txt", "a texture, as far as a copy is concerned");
      write("game/assets/vm/startup.ocmd", "on pre_load {\n  core log \"starting\" 2\n}\n");
      /// what a project folder gathers and a package must not have
      write("game/bin/Debug/Game.dll", "an old build");
      write("game/obj/Game.build-errors.log", "");
      write("game/logs/editor.log", "");
      write("game/.git/HEAD", "");
      write("game/Game.csproj", "<Project />");
      write("game/scripts/Thing.cs", "class Thing {}");

      /// stand-ins for the parts of the engine a compile only copies
      write("engine/runtime/other_runtime.exe", "the runtime");
      write("engine/runtime/other_runtime.pdb", "its symbols");
      write("engine/runtime/SDL3.dll", "a library");
      write("engine/runtime/notes.txt", "not a library");
      write("engine/assemblies/OtherCs.dll", "engine scripts");
      write("engine/assemblies/OtherCs.pdb", "their symbols");
      write("engine/assemblies/OtherCsBindings.dll", "bindings");
      write("engine/config/OtherCsBindings.runtimeconfig.json", "{}");
      write("engine/lua/driver.lua", "-- driver");
      write("engine/lua/console/commands.lua", "-- commands");
      write("engine/lua/readme.md", "not lua");
    }

    void TearDown() override {
      other_test::TearDown();
      events = nullptr;
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
    }

    /// path is from the sandbox
    void write(const filepath& path, const std::string_view text) {
      const filepath full = sandbox / path;
      std::filesystem::create_directories(full.parent_path());
      std::ofstream out(full, std::ios::binary | std::ios::trunc);
      ASSERT_TRUE(out.is_open()) << full.string();
      out.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    static std::string read(const filepath& path) {
      std::ifstream in(path, std::ios::binary);
      std::stringstream buffer;
      buffer << in.rdbuf();
      return buffer.str();
    }

    /// the engine's default pipeline under another name, as the collector's tests make one
    void write_pipeline(const filepath& path, const std::string_view name) {
      std::string text = read(resolve_asset_path(kDefaultProjectPipeline).resolved);
      const std::string_view old_name = "name = \"default-instancing\"";
      const size_t at = text.find(old_name);
      ASSERT_NE(at, std::string::npos);
      text.replace(at, old_name.size(), std::format("name = \"{}\"", name));
      write(path, text);
    }

    engine_sources stand_in_engine() const {
      return {
        .runtime_executable = sandbox / "engine/runtime/other_runtime.exe",
        .assemblies = sandbox / "engine/assemblies",
        .dotnet_runtime_config = sandbox / "engine/config/OtherCsBindings.runtimeconfig.json",
        .lua = sandbox / "engine/lua",
        .root = environment_root(),
      };
    }

    compile_settings settings(const package_config config = package_config::RELEASE) const {
      return { .project_file = project_folder, .output_directory = output, .config = config, .engine = stand_in_engine() };
    }

    static compile_report compile(const compile_settings& settings) {
      command_bus bus;
      register_builtin_command_devices(bus);
      return compile_project(bus, settings);
    }

    static bool any_holds(std::span<const std::string> errors, const std::string_view part) {
      return std::ranges::any_of(errors, [part](const std::string& error) { return error.contains(part); });
    }

    bool nothing_was_written() const {
      return !std::filesystem::exists(output);
    }

    /// the sandbox project with content: some of it used by the Main scene, some by nothing
    void write_content() {
      write("game/assets/scenes/main.oscn", kSceneWithContent);
      write("game/assets/models/crate.obj", kCrateModel);
      write("game/assets/models/crate.mtl", kCrateModelMaterial);
      write("game/assets/models/crate-skin.png", "the texture the model names");
      write("game/assets/materials/crate.omat", kCrateMaterial);
      write("game/assets/textures/paint.png", "the texture the material names");
      write("game/assets/audio/thud.wav", "a sound the scene plays");
      /// of the same kinds, and nothing uses them
      write("game/assets/models/statue.fbx", "a model no scene has");
      write("game/assets/textures/poster.png", "a texture nothing names");
      write("game/assets/audio/voice/line-1.wav", "a sound a script plays by path");
      write("game/assets/materials/old.omat", kCrateMaterial);
      write("game/assets/scenes/cut.oscn", kScene);
      /// of kinds a scene never names
      write("game/assets/data/levels.json", "{}");
      write("game/notes/todo.txt", "ship it");
    }

    asio::io_context io;
    std::unique_ptr<event_system> events;
    filepath sandbox;
    filepath project_folder;
    filepath output;
  };

  TEST_F(project_compiler_tests, a_package_holds_the_runtime_the_project_and_what_they_need_of_the_engine) {
    const compile_report report = compile(settings());
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    const filepath package = output / "Sandbox Game";
    EXPECT_EQ(report.package_root, package);

    /// the runtime under the game's name, with the libraries that sat beside it and nothing else
    EXPECT_EQ(read(package / "Sandbox Game.exe"), "the runtime");
    EXPECT_EQ(read(package / "SDL3.dll"), "a library");
    EXPECT_FALSE(std::filesystem::exists(package / "notes.txt"));
    EXPECT_FALSE(std::filesystem::exists(package / "other_runtime.pdb"));

    /// the config the runtime finds beside itself
    const std::string config = read(package / "Sandbox Game.toml");
    EXPECT_TRUE(config.contains("boot-file = \"game/boot.obin\""));
    EXPECT_TRUE(config.contains("name = \"Sandbox Game\""));
    EXPECT_TRUE(config.contains("core-log-level = \"info\""));
    EXPECT_TRUE(config.contains("core-log-file = \"logs/Sandbox Game.log\""));
    EXPECT_TRUE(config.contains("main-pipeline = \"default-instancing\""));
    EXPECT_TRUE(config.contains("pipelines = [ \"engine://resources/editor-assets/default-pipeline.toml\" ]"));
    EXPECT_TRUE(config.contains("other-lua-directory = \"engine/lua\""));
    EXPECT_FALSE(config.contains("vm-debug-mode"));

    /// of the engine: the pipeline and what it reaches, the fonts, the scripts side
    const filepath engine = package / "engine";
    EXPECT_TRUE(std::filesystem::is_regular_file(engine / "resources/editor-assets/default-pipeline.toml"));
    EXPECT_TRUE(std::filesystem::is_regular_file(engine / "resources/basic-shading.frag"));
    EXPECT_TRUE(std::filesystem::is_regular_file(engine / "resources/shader-modules/camera.glsl"));
    EXPECT_TRUE(std::filesystem::is_regular_file(engine / "resources/smaa-area-tex.bin"));
    EXPECT_FALSE(std::filesystem::is_empty(engine / "resources/fonts"));
    /// and not what no pipeline of this project reaches
    EXPECT_FALSE(std::filesystem::exists(engine / "resources/editor.lua"));
    EXPECT_FALSE(std::filesystem::exists(engine / "resources/models"));
    EXPECT_EQ(read(engine / "lua/driver.lua"), "-- driver");
    EXPECT_EQ(read(engine / "lua/console/commands.lua"), "-- commands");
    EXPECT_FALSE(std::filesystem::exists(engine / "lua/readme.md"));
    EXPECT_EQ(read(engine / "dotnet-assemblies/OtherCs.dll"), "engine scripts");
    EXPECT_EQ(read(engine / "dotnet-assemblies/OtherCsBindings.dll"), "bindings");
    EXPECT_EQ(read(engine / "dotnet-assemblies/OtherCsBindings.runtimeconfig.json"), "{}");
    EXPECT_FALSE(std::filesystem::exists(engine / "dotnet-assemblies/OtherCs.pdb"));

    /// the project: its content, its boot program, and none of its sources or build leftovers
    const filepath game = package / "game";
    EXPECT_TRUE(std::filesystem::is_regular_file(game / "assets/scenes/main.oscnb"));
    EXPECT_EQ(read(game / "assets/textures/wall.txt"), "a texture, as far as a copy is concerned");
    EXPECT_EQ(read(game / "project.lua"), "-- project settings\n");
    EXPECT_FALSE(ocmd_toolchain{}.read_obin(game / "boot.obin").empty());
    for (const std::string_view left_out : { "assets/vm", "bin", "obj", "logs", ".git", "Game.csproj", "scripts/Thing.cs" }) {
      EXPECT_FALSE(std::filesystem::exists(game / left_out)) << left_out;
    }

    /// the project file as the package needs it, comments and all
    const std::string packaged = read(game / "game.toml");
    EXPECT_TRUE(packaged.starts_with("# a comment that has to survive the packaging\n"));
    EXPECT_TRUE(packaged.contains("[vm]\nboot-file = \"boot.obin\"\n"));
    EXPECT_TRUE(packaged.contains("starting-scene = \"Main\"\n"));
    EXPECT_EQ(read(project_folder / "game.toml"), kProject);

    EXPECT_TRUE(std::filesystem::exists(package / kPackageMarker));
    EXPECT_FALSE(std::filesystem::exists(output / ".staging"));
    EXPECT_TRUE(report.warnings.empty());
  }

  TEST_F(project_compiler_tests, the_projects_plugins_are_packaged_in_the_packages_own_configuration) {
    /// built outside the project's folder, one folder to a configuration, as CMake leaves them
    write("build/game/Release/systems.dll", "release systems");
    write("build/game/Release/systems.pdb", "release symbols");
    write("build/game/Debug/systems.dll", "debug systems");
    write("build/game/Debug/systems.pdb", "debug symbols");
    write("game/tools/fixed.dll", "one build only");
    const std::string with_plugins = std::format(
      "[project]\nplugins = [\n"
      "  {{ name = \"Game Systems\", path = \"build/game/${{build-config}}/systems.dll\" }},  # the game's own\n"
      "  {{ name = \"Fixed\", path = 'game/tools/fixed.dll' }}\n"
      "]\n{}",
      std::string_view{ kProject }.substr(std::string_view{ kProject }.find("metadata")));
    write("game/game.toml", with_plugins);

    compile_report report = compile(settings());
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    filepath game = report.package_root / "game";
    EXPECT_EQ(read(game / "bin/systems.dll"), "release systems");
    EXPECT_EQ(read(game / "bin/fixed.dll"), "one build only");
    EXPECT_FALSE(std::filesystem::exists(game / "bin/systems.pdb"));
    /// the packaged file points at the copies; the rest of each line is as it was
    std::string packaged = read(game / "game.toml");
    EXPECT_TRUE(packaged.contains("  { name = \"Game Systems\", path = \"game/bin/systems.dll\" },  # the game's own\n"));
    EXPECT_TRUE(packaged.contains("  { name = \"Fixed\", path = \"game/bin/fixed.dll\" }\n"));
    EXPECT_FALSE(packaged.contains("build-config"));
    EXPECT_EQ(read(project_folder / "game.toml"), with_plugins);

    /// a Debug package takes the Debug build, whatever this test was built as, and its symbols
    report = compile(settings(package_config::DEBUG));
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    game = report.package_root / "game";
    EXPECT_EQ(read(game / "bin/systems.dll"), "debug systems");
    EXPECT_EQ(read(game / "bin/systems.pdb"), "debug symbols");
  }

  TEST_F(project_compiler_tests, a_plugin_that_cannot_be_packaged_stops_the_compile_before_it_starts) {
    const std::string_view rest = std::string_view{ kProject }.substr(std::string_view{ kProject }.find("metadata"));
    write("build/game/Debug/systems.dll", "debug only");

    /// not built in the configuration being packaged
    write("game/game.toml", std::format("[project]\nplugins = [ {{ name = \"Game Systems\", path = \"build/game/${{build-config}}/systems.dll\" }} ]\n{}", rest));
    compile_report report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "plugin 'Game Systems' is listed as 'build/game/${build-config}/systems.dll', and there is no Release build of it at 'build/game/Release/systems.dll'"));
    EXPECT_TRUE(nothing_was_written());

    /// two that would land on each other in the package
    write("game/other/systems.dll", "another");
    write("game/game.toml", std::format("[project]\nplugins = [ {{ name = \"One\", path = \"build/game/Debug/systems.dll\" }}, {{ name = \"Two\", path = \"game/other/systems.dll\" }} ]\n{}", rest));
    report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "two plugins are both named 'systems.dll'"));

    write("game/game.toml", std::format("[project]\nplugins = [ {{ name = \"No Path\" }} ]\n{}", rest));
    report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "has no name or no path"));
    EXPECT_TRUE(nothing_was_written());
  }

  TEST_F(project_compiler_tests, a_debug_package_has_its_own_folder_its_symbols_and_a_louder_log) {
    const compile_report report = compile(settings(package_config::DEBUG));
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    const filepath package = output / "Sandbox Game-debug";
    EXPECT_EQ(report.package_root, package);

    EXPECT_EQ(read(package / "Sandbox Game.exe"), "the runtime");
    EXPECT_EQ(read(package / "other_runtime.pdb"), "its symbols");
    EXPECT_EQ(read(package / "engine/dotnet-assemblies/OtherCs.pdb"), "their symbols");
    const std::string config = read(package / "Sandbox Game.toml");
    EXPECT_TRUE(config.contains("core-log-level = \"debug\""));
    EXPECT_TRUE(config.contains("vm-debug-mode = false"));
    /// a Debug package has the engine's console, opened by its key
    EXPECT_TRUE(config.contains("[ui]\nenable = true\nconsole-key = true\n"));
    /// said every time, since it is the first thing that goes wrong on another machine
    EXPECT_TRUE(any_holds(report.warnings, "needs the debug C runtime"));

    /// and a Release compile beside it does not disturb it
    ASSERT_TRUE(compile(settings()).success);
    EXPECT_TRUE(std::filesystem::exists(package / "Sandbox Game.exe"));
    EXPECT_TRUE(std::filesystem::exists(output / "Sandbox Game" / "Sandbox Game.exe"));
    /// which draws no UI at all
    const std::string release_config = read(output / "Sandbox Game" / "Sandbox Game.toml");
    EXPECT_TRUE(release_config.contains("[ui]\nenable = false\n"));
    EXPECT_FALSE(release_config.contains("console-key"));
  }

  TEST_F(project_compiler_tests, a_project_says_whether_its_package_has_the_console) {
    /// a Release package with it, a Debug package without
    write("game/game.toml", std::format("{}\n[package]\nconsole = true\n", kProject));
    ASSERT_TRUE(compile(settings()).success);
    EXPECT_TRUE(read(output / "Sandbox Game" / "Sandbox Game.toml").contains("[ui]\nenable = true\nconsole-key = true\n"));

    write("game/game.toml", std::format("{}\n[package]\nconsole = false\n", kProject));
    ASSERT_TRUE(compile(settings(package_config::DEBUG)).success);
    const std::string config = read(output / "Sandbox Game-debug" / "Sandbox Game.toml");
    EXPECT_TRUE(config.contains("[ui]\nenable = false\n"));
    EXPECT_FALSE(config.contains("console-key"));

    /// and it is true or false, nothing else
    write("game/game.toml", std::format("{}\n[package]\nconsole = \"yes\"\n", kProject));
    const compile_report report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "[package] console is true or false"));
  }

  TEST_F(project_compiler_tests, the_public_name_is_the_callers_then_the_projects_then_its_metadata_then_its_folder) {
    compile_settings named = settings();
    named.public_name = "Chosen Here";
    ASSERT_TRUE(compile(named).success);
    EXPECT_TRUE(std::filesystem::exists(output / "Chosen Here" / "Chosen Here.exe"));
    /// the folder inside the package keeps the project's own name, whatever the game is called
    EXPECT_TRUE(std::filesystem::exists(output / "Chosen Here" / "game" / "game.toml"));

    write("game/game.toml", set_toml_string(kProject, "project", "public-name", "From The File"));
    ASSERT_TRUE(compile(settings()).success);
    EXPECT_TRUE(std::filesystem::exists(output / "From The File" / "From The File.exe"));

    write("game/game.toml", "[scene-graph]\nstarting-scene = \"Main\"\nscenes = [ { name = \"Main\", path = \"game/assets/scenes/main.oscn\" } ]\n");
    ASSERT_TRUE(compile(settings()).success);
    EXPECT_TRUE(std::filesystem::exists(output / "game" / "game.exe"));
  }

  TEST_F(project_compiler_tests, a_public_name_has_to_be_a_file_name) {
    for (const std::string_view bad : { "", "a/b", "a\\b", "what?", "star*", "quo\"te", "pipe|d", "<tag>", "trailing.", "trailing ", " leading", "CON", "nul", "Com1", "LPT9.txt" }) {
      EXPECT_TRUE(invalid_public_name_reason(bad).has_value()) << "'" << bad << "'";
    }
    for (const std::string_view good : { "My Game", "game-2", "Console", "COM10", "a.b", "über" }) {
      EXPECT_FALSE(invalid_public_name_reason(good).has_value()) << "'" << good << "'";
    }

    compile_settings named = settings();
    named.public_name = "What?";
    const compile_report report = compile(named);
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "the public name 'What?' cannot be used"));
    EXPECT_TRUE(nothing_was_written());
  }

  TEST_F(project_compiler_tests, a_package_needs_a_starting_scene_the_project_has) {
    const std::string no_start = remove_toml_key(kProject, "scene-graph", "starting-scene");
    write("game/game.toml", no_start);
    compile_report report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "the project has no starting scene; choose one of: Main, Second"));
    EXPECT_TRUE(nothing_was_written());

    /// the caller's choice goes into the package and not back into the project
    compile_settings chosen = settings();
    chosen.starting_scene = "Second";
    report = compile(chosen);
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    EXPECT_TRUE(read(report.package_root / "game" / "game.toml").contains("[scene-graph]\nstarting-scene = \"Second\"\n"));
    EXPECT_EQ(read(project_folder / "game.toml"), no_start);

    chosen.starting_scene = "Nowhere";
    report = compile(chosen);
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "the starting scene 'Nowhere' is not one of the project's scenes: Main, Second"));
  }

  TEST_F(project_compiler_tests, a_project_with_no_scenes_or_a_scene_that_is_missing_is_not_packaged) {
    write("game/game.toml", "[project]\n");
    compile_report report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "the project has no scenes"));

    write("game/game.toml", kProject);
    std::filesystem::remove(project_folder / "assets/scenes/second.oscn");
    report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "scene 'Second' is listed as 'game/assets/scenes/second.oscn', which does not exist"));

    /// a scene kept somewhere else would not be in the package
    write("elsewhere/outside.oscn", kScene);
    write("game/game.toml", "[scene-graph]\nstarting-scene = \"Out\"\nscenes = [ { name = \"Out\", path = \"elsewhere/outside.oscn\" } ]\n");
    report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "outside the project's folder"));
    EXPECT_TRUE(nothing_was_written());
  }

  TEST_F(project_compiler_tests, a_failed_compile_leaves_the_last_good_package_as_it_was) {
    ASSERT_TRUE(compile(settings()).success);
    const filepath package = output / "Sandbox Game";
    const std::string good_program = read(package / "game" / "boot.obin");

    write("game/assets/vm/startup.ocmd", "on pre_load {\n  this is not a command\n}\n");
    const compile_report report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "the project's VM sources did not build"));
    EXPECT_TRUE(report.package_root.empty());

    EXPECT_EQ(read(package / "game" / "boot.obin"), good_program);
    EXPECT_TRUE(std::filesystem::exists(package / "Sandbox Game.exe"));
    EXPECT_FALSE(std::filesystem::exists(output / ".staging"));
  }

  TEST_F(project_compiler_tests, a_compile_replaces_its_own_package_and_no_other_folder) {
    /// someone's own folder happens to have the game's name
    write("out/Sandbox Game/save-games/slot-1.dat", "hours of play");
    compile_report report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "was not made by a compile"));
    EXPECT_EQ(read(output / "Sandbox Game/save-games/slot-1.dat"), "hours of play");
    EXPECT_FALSE(std::filesystem::exists(output / ".staging"));

    /// its own it replaces whole: what the last package had and this one does not is gone
    std::filesystem::remove_all(output);
    ASSERT_TRUE(compile(settings()).success);
    write("out/Sandbox Game/game/assets/textures/stale.txt", "from an older package");
    ASSERT_TRUE(compile(settings()).success);
    EXPECT_FALSE(std::filesystem::exists(output / "Sandbox Game/game/assets/textures/stale.txt"));
    EXPECT_TRUE(std::filesystem::exists(output / "Sandbox Game/game/assets/textures/wall.txt"));
  }

  TEST_F(project_compiler_tests, the_package_cannot_be_made_inside_the_project) {
    compile_settings inside = settings();
    inside.output_directory = project_folder / "packages";
    const compile_report report = compile(inside);
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "is inside the project"));
    EXPECT_FALSE(std::filesystem::exists(project_folder / "packages"));
  }

  TEST_F(project_compiler_tests, an_assembly_the_project_already_has_ships_as_it_is) {
    write("game/prebuilt/Game.dll", "built somewhere else");
    write("game/game.toml", set_toml_string(kProject, "scripting", "assembly", "game/prebuilt/Game.dll"));

    const compile_report report = compile(settings());
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    const filepath game = report.package_root / "game";
    EXPECT_EQ(read(game / "bin/Game.dll"), "built somewhere else");
    EXPECT_TRUE(read(game / "game.toml").contains("assembly = \"game/bin/Game.dll\"\n"));

    write("game/game.toml", set_toml_string(kProject, "scripting", "assembly", "game/prebuilt/Gone.dll"));
    const compile_report missing = compile(settings());
    EXPECT_FALSE(missing.success);
    EXPECT_TRUE(any_holds(missing.errors, "the project's script assembly, 'game/prebuilt/Gone.dll', does not exist"));
  }

  TEST_F(project_compiler_tests, a_projects_own_pipeline_is_listed_from_its_folder_and_the_main_one_has_to_be_known) {
    write_pipeline("game/assets/pipelines/mine.toml", "mine");
    const std::string two = std::format("{}\n[rendering]\npipelines = [ \"assets/pipelines/mine.toml\", \"{}\" ]\n", kProject, kDefaultProjectPipeline);
    write("game/game.toml", two);

    compile_report report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "does not say which one reaches the window; choose one of: mine, default-instancing"));

    compile_settings chosen = settings();
    chosen.main_pipeline = "mine";
    report = compile(chosen);
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    const std::string config = read(report.package_root / "Sandbox Game.toml");
    EXPECT_TRUE(config.contains("main-pipeline = \"mine\""));
    EXPECT_TRUE(config.contains("pipelines = [ \"game/assets/pipelines/mine.toml\", \"engine://resources/editor-assets/default-pipeline.toml\" ]"));
    /// the project's pipeline travels with the project, and the engine shaders it uses with the engine
    EXPECT_TRUE(std::filesystem::is_regular_file(report.package_root / "game/assets/pipelines/mine.toml"));
    EXPECT_TRUE(std::filesystem::is_regular_file(report.package_root / "engine/resources/basic-shading.frag"));

    write("game/game.toml", std::format("{}main-pipeline = \"missing\"\n", two));
    report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "the main pipeline 'missing' is not one of the project's pipelines"));
  }

  TEST_F(project_compiler_tests, a_runtime_that_has_not_been_built_stops_the_compile_before_it_starts) {
    compile_settings no_runtime = settings();
    no_runtime.engine->runtime_executable = sandbox / "engine/runtime/gone.exe";
    const compile_report report = compile(no_runtime);
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "the runtime to package"));
    EXPECT_TRUE(nothing_was_written());

    compile_settings no_project = settings();
    no_project.project_file = sandbox / "nothing-here";
    EXPECT_FALSE(compile(no_project).success);
    EXPECT_TRUE(nothing_was_written());
  }

  TEST_F(project_compiler_tests, a_value_in_a_project_file_is_set_without_disturbing_the_rest) {
    const std::string_view text = "# top\n[vm]\n  boot-file = \"assets/vm/boot.oasm\"  \n# between\n[scripting]\ncs_project = \"game/Game.csproj\"\n";

    /// replaced where it is, with its indent
    EXPECT_EQ(set_toml_string(text, "vm", "boot-file", "boot.obin"),
              "# top\n[vm]\n  boot-file = \"boot.obin\"\n# between\n[scripting]\ncs_project = \"game/Game.csproj\"\n");
    /// added under the table's header when the table has no such key
    EXPECT_EQ(set_toml_string(text, "scripting", "assembly", "game/bin/Game.dll"),
              "# top\n[vm]\n  boot-file = \"assets/vm/boot.oasm\"  \n# between\n[scripting]\nassembly = \"game/bin/Game.dll\"\ncs_project = \"game/Game.csproj\"\n");
    /// a key of the same name in another table is not this table's
    EXPECT_EQ(set_toml_string(text, "rendering", "boot-file", "x"), std::format("{}\n[rendering]\nboot-file = \"x\"\n", text));
    /// and a table that is not there is added at the end, to a file with or without a last line break
    EXPECT_EQ(set_toml_string("[a]\nb = 1", "c", "d", "e"), "[a]\nb = 1\n\n[c]\nd = \"e\"\n");
    EXPECT_EQ(set_toml_string("", "c", "d", "e"), "[c]\nd = \"e\"\n");

    EXPECT_EQ(remove_toml_key(text, "scripting", "cs_project"), "# top\n[vm]\n  boot-file = \"assets/vm/boot.oasm\"  \n# between\n[scripting]\n");
    EXPECT_EQ(remove_toml_key(text, "vm", "cs_project"), text);
    EXPECT_EQ(remove_toml_key(text, "nowhere", "cs_project"), text);

    /// the file's own line endings are the ones used, and a value is written as a TOML string
    EXPECT_EQ(set_toml_string("[a]\r\nb = \"old\"\r\n", "a", "c", "say \"hi\""), "[a]\r\nc = \"say \\\"hi\\\"\"\r\nb = \"old\"\r\n");
    EXPECT_EQ(set_toml_string("[a]\r\nb = \"old\"\r\n", "a", "b", "new"), "[a]\r\nb = \"new\"\r\n");
    /// a header with a comment after it is still the header
    EXPECT_EQ(set_toml_string("[a] # the a table\nb = 1\n", "a", "b", "2"), "[a] # the a table\nb = \"2\"\n");
  }

  TEST_F(project_compiler_tests, a_preview_says_what_a_compile_would_use_and_writes_nothing) {
    command_bus bus;
    register_builtin_command_devices(bus);
    const compile_preview preview = preview_project_compile(bus, project_folder);

    EXPECT_TRUE(preview.problems.empty()) << (preview.problems.empty() ? "" : preview.problems.front());
    EXPECT_EQ(preview.public_name, "Sandbox Game");
    EXPECT_EQ(preview.scenes, (ostd::vector<std::string>{ "Main", "Second" }));
    EXPECT_EQ(preview.starting_scene, "Main");
    EXPECT_EQ(preview.pipelines, (ostd::vector<std::string>{ "default-instancing" }));
    EXPECT_EQ(preview.main_pipeline, "default-instancing");
    EXPECT_EQ(preview.vm_sources, (ostd::vector<std::string>{ "assets/vm/startup.ocmd" }));
    ASSERT_EQ(preview.hooks.size(), 1);
    EXPECT_EQ(preview.hooks.front().hook, hook_id::PRE_LOAD);
    EXPECT_EQ(preview.hooks.front().unit, "assets/vm/startup.ocmd");

    /// the boot program was built in memory
    EXPECT_TRUE(nothing_was_written());
    EXPECT_FALSE(std::filesystem::exists(project_folder / "boot.obin"));
    EXPECT_EQ(read(project_folder / "game.toml"), kProject);
  }

  TEST_F(project_compiler_tests, a_preview_leaves_open_what_the_project_does_not_decide) {
    command_bus bus;
    register_builtin_command_devices(bus);

    /// a starting scene the project does not have is no starting scene
    write("game/game.toml", set_toml_string(kProject, "scene-graph", "starting-scene", "Gone"));
    compile_preview preview = preview_project_compile(bus, project_folder);
    EXPECT_TRUE(preview.starting_scene.empty());
    EXPECT_TRUE(preview.problems.empty());

    /// several pipelines and no word on which one reaches the window
    write_pipeline("game/assets/pipelines/mine.toml", "mine");
    const std::string two = std::format("{}\n[rendering]\npipelines = [ \"assets/pipelines/mine.toml\", \"{}\" ]\n", kProject, kDefaultProjectPipeline);
    write("game/game.toml", two);
    preview = preview_project_compile(bus, project_folder);
    EXPECT_EQ(preview.pipelines, (ostd::vector<std::string>{ "mine", "default-instancing" }));
    EXPECT_TRUE(preview.main_pipeline.empty());
    EXPECT_TRUE(preview.problems.empty());

    write("game/game.toml", std::format("{}main-pipeline = \"mine\"\n", two));
    EXPECT_EQ(preview_project_compile(bus, project_folder).main_pipeline, "mine");

    /// the name the project gives itself wins over its metadata
    write("game/game.toml", set_toml_string(kProject, "project", "public-name", "From The File"));
    EXPECT_EQ(preview_project_compile(bus, project_folder).public_name, "From The File");
  }

  TEST_F(project_compiler_tests, a_preview_names_what_would_stop_any_compile) {
    command_bus bus;
    register_builtin_command_devices(bus);

    write("game/assets/vm/startup.ocmd", "on pre_load {\n  no such command\n}\n");
    compile_preview preview = preview_project_compile(bus, project_folder);
    EXPECT_TRUE(any_holds(preview.problems, "the project's VM sources do not build"));
    /// and still says the rest
    EXPECT_EQ(preview.scenes.size(), 2);
    EXPECT_EQ(preview.starting_scene, "Main");

    write("game/assets/vm/startup.ocmd", "");
    write("game/game.toml", "[project]\n");
    preview = preview_project_compile(bus, project_folder);
    EXPECT_TRUE(any_holds(preview.problems, "the project has no scenes"));

    write("game/game.toml", "this is not toml = = =");
    EXPECT_TRUE(any_holds(preview_project_compile(bus, project_folder).problems, "is not a project file"));
    EXPECT_TRUE(any_holds(preview_project_compile(bus, sandbox / "nothing-here").problems, "cannot be read"));
  }

  TEST_F(project_compiler_tests, choices_are_written_into_the_project_file_only_where_they_differ) {
    const filepath file = project_folder / "game.toml";
    const auto written_at = std::filesystem::last_write_time(file);

    /// what the file already gives, by a key or by its metadata, and what is left empty
    EXPECT_FALSE(save_compile_choices(project_folder, { .public_name = "Sandbox Game", .starting_scene = "Main" }).has_value());
    EXPECT_FALSE(save_compile_choices(project_folder, {}).has_value());
    EXPECT_EQ(read(file), kProject);
    EXPECT_EQ(std::filesystem::last_write_time(file), written_at);

    EXPECT_FALSE(save_compile_choices(project_folder, { .public_name = "Renamed", .starting_scene = "Second", .main_pipeline = "mine" }).has_value());
    const std::string text = read(file);
    EXPECT_TRUE(text.starts_with("# a comment that has to survive the packaging\n[project]\npublic-name = \"Renamed\"\n"));
    EXPECT_TRUE(text.contains("starting-scene = \"Second\""));
    EXPECT_FALSE(text.contains("starting-scene = \"Main\""));
    EXPECT_TRUE(text.ends_with("\n[rendering]\nmain-pipeline = \"mine\"\n"));
    /// the scene list is as it was
    EXPECT_TRUE(text.contains("  { name = \"Second\", path = \"game/assets/scenes/second.oscn\" },\n"));

    /// and the next compile takes them from the file
    command_bus bus;
    register_builtin_command_devices(bus);
    const compile_preview preview = preview_project_compile(bus, project_folder);
    EXPECT_EQ(preview.public_name, "Renamed");
    EXPECT_EQ(preview.starting_scene, "Second");

    /// once they are in the file, writing them again changes nothing
    const std::string after = read(file);
    EXPECT_FALSE(save_compile_choices(project_folder, { .public_name = "Renamed", .starting_scene = "Second", .main_pipeline = "mine" }).has_value());
    EXPECT_EQ(read(file), after);

    const opt<std::string> missing = save_compile_choices(sandbox / "nothing-here", { .public_name = "x" });
    ASSERT_TRUE(missing.has_value());
    EXPECT_TRUE(missing->contains("cannot be read"));
  }

  TEST_F(project_compiler_tests, listed_scenes_are_packaged_in_binary_and_the_project_file_points_at_them) {
    const compile_report report = compile(settings());
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    const filepath game = report.package_root / "game";

    /// the binary form in the place of the text, and it reads back as the same scene
    for (const std::string_view scene : { "main", "second" }) {
      EXPECT_FALSE(std::filesystem::exists(game / "assets/scenes" / std::format("{}.oscn", scene))) << scene;
      const serialization::scene_parse_result packaged = serialization::load_scene_document(game / "assets/scenes" / std::format("{}.oscnb", scene));
      ASSERT_TRUE(packaged.success()) << scene << ": " << packaged.error;
      EXPECT_EQ(packaged.document->name, "main");
    }
    const std::string text = read(game / "game.toml");
    EXPECT_TRUE(text.contains("  { name = \"Main\", path = \"game/assets/scenes/main.oscnb\" },\n"));
    EXPECT_TRUE(text.contains("  { name = \"Second\", path = \"game/assets/scenes/second.oscnb\" },\n"));
    EXPECT_FALSE(text.contains(".oscn\""));
    /// and the project itself is as it was
    EXPECT_EQ(read(project_folder / "assets/scenes/main.oscn"), kScene);
    EXPECT_EQ(read(project_folder / "game.toml"), kProject);
  }

  TEST_F(project_compiler_tests, a_scene_that_cannot_be_read_stops_the_compile_before_it_starts) {
    write("game/assets/scenes/second.oscn", "[scene\nthis is not a scene");
    const compile_report report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "scene 'Second' ('game/assets/scenes/second.oscn') cannot be read"));
    EXPECT_TRUE(nothing_was_written());
  }

  TEST_F(project_compiler_tests, two_scenes_that_would_be_one_file_in_the_package_are_refused) {
    /// the same scene twice, once already in binary
    const serialization::scene_parse_result scene = serialization::load_scene_document(project_folder / "assets/scenes/main.oscn");
    ASSERT_TRUE(scene.success());
    ASSERT_TRUE(serialization::save_scene_document(*scene.document, project_folder / "assets/scenes/second.oscnb"));
    std::string text{ kProject };
    const std::string_view listed = "game/assets/scenes/second.oscn\"";
    text.replace(text.find(listed), listed.size(), "game/assets/scenes/second.oscnb\" },\n  { name = \"Third\", path = \"game/assets/scenes/second.oscn\"");
    write("game/game.toml", text);

    const compile_report report = compile(settings());
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(any_holds(report.errors, "scenes 'Second' and 'Third' would both be packaged as 'assets/scenes/second.oscnb'"));
    EXPECT_TRUE(nothing_was_written());
  }

  TEST_F(project_compiler_tests, content_that_nothing_reaches_is_left_out_and_everything_else_is_copied) {
    write_content();
    const compile_report report = compile(settings());
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    const filepath game = report.package_root / "game";

    /// what the scene names, what its material names, and what its model names
    for (const std::string_view used : { "assets/models/crate.obj", "assets/materials/crate.omat", "assets/audio/thud.wav", "assets/textures/paint.png", "assets/models/crate-skin.png" }) {
      EXPECT_TRUE(std::filesystem::is_regular_file(game / used)) << used;
    }
    /// files of kinds no scene names are copied whoever uses them: a script may read them
    for (const std::string_view other : { "assets/models/crate.mtl", "assets/data/levels.json", "notes/todo.txt", "assets/textures/wall.txt", "project.lua" }) {
      EXPECT_TRUE(std::filesystem::is_regular_file(game / other)) << other;
    }

    ostd::vector<std::string> left_out = report.left_out;
    std::ranges::sort(left_out);
    EXPECT_EQ(left_out, (ostd::vector<std::string>{
                          "assets/audio/voice/line-1.wav",
                          "assets/materials/old.omat",
                          "assets/models/statue.fbx",
                          "assets/scenes/cut.oscn",
                          "assets/textures/poster.png",
                        }));
    uint64_t bytes = 0;
    for (const std::string& name : left_out) {
      EXPECT_FALSE(std::filesystem::exists(game / name)) << name;
      bytes += std::filesystem::file_size(project_folder / name);
    }
    EXPECT_EQ(report.left_out_bytes, bytes);
    /// and a size is said in the unit that fits it
    EXPECT_EQ(readable_size(182), "182 bytes");
    EXPECT_EQ(readable_size(1536), "1.5 KB");
    EXPECT_EQ(readable_size(5 * 1024 * 1024), "5.0 MB");
    /// leaving something out is not a warning; only the Release one about nothing is here
    EXPECT_TRUE(report.warnings.empty()) << report.warnings.front();
  }

  TEST_F(project_compiler_tests, keep_ships_what_the_game_loads_by_path_and_exclude_ships_nothing_it_names) {
    write_content();
    write("game/game.toml", std::format("{}\n[package]\n"
                                        "keep = [ \"assets/audio/**\", \"assets/scenes/main.oscn\", \"assets/*/poster.png\" ]\n"
                                        "exclude = [ \"notes/**\", \"**/*.json\", \"assets/textures/paint.png\" ]\n",
                                        kProject));
    const compile_report report = compile(settings());
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    const filepath game = report.package_root / "game";

    EXPECT_EQ(read(game / "assets/audio/voice/line-1.wav"), "a sound a script plays by path");
    EXPECT_TRUE(std::filesystem::is_regular_file(game / "assets/textures/poster.png"));
    /// a listed scene that is kept ships as text beside its binary copy
    EXPECT_EQ(read(game / "assets/scenes/main.oscn"), kSceneWithContent);
    EXPECT_TRUE(std::filesystem::is_regular_file(game / "assets/scenes/main.oscnb"));
    EXPECT_FALSE(std::filesystem::exists(game / "assets/scenes/second.oscn"));

    /// excluded files are not there, whatever kind they are, and are not counted as left out
    EXPECT_FALSE(std::filesystem::exists(game / "notes"));
    EXPECT_FALSE(std::filesystem::exists(game / "assets/data/levels.json"));
    EXPECT_FALSE(std::filesystem::exists(game / "assets/textures/paint.png"));
    ostd::vector<std::string> left_out = report.left_out;
    std::ranges::sort(left_out);
    EXPECT_EQ(left_out, (ostd::vector<std::string>{ "assets/materials/old.omat", "assets/models/statue.fbx", "assets/scenes/cut.oscn" }));
    /// excluding something the package uses is allowed, and said
    EXPECT_TRUE(any_holds(report.warnings, "'assets/textures/paint.png' is left out by [package] exclude, and something in the package uses it"));
    EXPECT_EQ(report.warnings.size(), 1);
  }

  TEST_F(project_compiler_tests, an_id_written_through_the_working_directory_is_given_the_mount_of_its_file) {
    write_content();
    /// as an editor started beside the project's folder used to save a scene
    write("game/assets/scenes/main.oscn",
          "[scene]\n"
          "schema-version = 1\n"
          "name = \"main\"\n"
          "\n"
          "[[objects]]\n"
          "id = 1\n"
          "name = \"Crate\"\n"
          "[objects.components.render]\n"
          "model_asset_id = \"cwd://game/assets/models/crate.obj\"\n"
          "material_asset_id = \"cwd://game/assets/materials/crate.omat\"\n"
          "[objects.components.audio-source]\n"
          "clip_asset_id = \"game/assets/audio/thud.wav\"\n"
          "\n"
          "[[objects]]\n"
          "id = 2\n"
          "name = \"Gone\"\n"
          "[objects.components.render]\n"
          "model_asset_id = \"cwd://nowhere/gone.fbx\"\n");
    const compile_report report = compile(settings());
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    const filepath game = report.package_root / "game";

    const serialization::scene_parse_result packaged = serialization::load_scene_document(game / "assets/scenes/main.oscnb");
    ASSERT_TRUE(packaged.success()) << packaged.error;
    ostd::vector<std::string> ids;
    for (const serialization::component_asset_ref& ref : serialization::collect_scene_asset_refs(*packaged.document)) {
      ids.push_back(ref.path);
    }
    /// the two that lead to the project's files name its mount; one that was never through
    ///  the working directory is as it was; one that leads nowhere is left, and said
    EXPECT_EQ(ids, (ostd::vector<std::string>{
                     "game://assets/models/crate.obj",
                     "game://assets/materials/crate.omat",
                     "game/assets/audio/thud.wav",
                     "cwd://nowhere/gone.fbx",
                   }));
    EXPECT_TRUE(any_holds(report.warnings, "scene 'Main' names 'cwd://nowhere/gone.fbx' through the working directory"));
    /// and what they lead to is in the package, with what the model and the material name
    for (const std::string_view used : { "assets/models/crate.obj", "assets/models/crate-skin.png", "assets/materials/crate.omat", "assets/textures/paint.png", "assets/audio/thud.wav" }) {
      EXPECT_TRUE(std::filesystem::is_regular_file(game / used)) << used;
    }
    /// the project's own scene is not rewritten
    EXPECT_TRUE(read(project_folder / "assets/scenes/main.oscn").contains("cwd://game/assets/models/crate.obj"));
  }

  TEST_F(project_compiler_tests, keeping_everything_packages_the_project_as_it_is) {
    write_content();
    write("game/game.toml", std::format("{}\n[package]\nkeep = [ \"**\" ]\n", kProject));
    const compile_report report = compile(settings());
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    EXPECT_TRUE(report.left_out.empty());
    EXPECT_EQ(report.left_out_bytes, 0);
    const filepath game = report.package_root / "game";
    for (const std::string_view kept : { "assets/models/statue.fbx", "assets/textures/poster.png", "assets/materials/old.omat", "assets/scenes/cut.oscn", "assets/scenes/second.oscn" }) {
      EXPECT_TRUE(std::filesystem::is_regular_file(game / kept)) << kept;
    }
  }

  TEST_F(project_compiler_tests, a_package_pattern_that_cannot_be_read_is_refused_before_anything_is_written) {
    const auto refused = [this](const std::string_view rules, const std::string_view reason) {
      write("game/game.toml", std::format("{}\n[package]\n{}\n", kProject, rules));
      const compile_report report = compile(settings());
      EXPECT_FALSE(report.success) << rules;
      EXPECT_TRUE(any_holds(report.errors, reason)) << rules << ": " << (report.errors.empty() ? "" : report.errors.front());
      EXPECT_TRUE(nothing_was_written()) << rules;
    };
    refused("keep = [ 'assets\\audio' ]", "[package] keep has the pattern 'assets\\audio', which cannot be used: folders are separated by '/'");
    refused("keep = [ \"/assets\" ]", "with no '/' at either end");
    refused("exclude = [ \"assets/**.png\" ]", "[package] exclude has the pattern 'assets/**.png', which cannot be used: '**' has to be a whole folder name");
    refused("exclude = [ \"assets//x\" ]", "it has an empty folder name");
    refused("keep = [ \"assets/{a,b}\" ]", "braces and brackets are not supported");
    refused("keep = [ \"\" ]", "it is empty");
    refused("keep = \"assets/**\"", "[package] keep is a list of patterns");
    refused("keep = [ 3 ]", "[package] keep holds something that is not a pattern in quotes");
  }

  TEST_F(project_compiler_tests, a_preview_lists_what_a_compile_would_leave_out_and_writes_nothing) {
    write_content();
    command_bus bus;
    register_builtin_command_devices(bus);
    compile_preview preview = preview_project_compile(bus, project_folder);
    EXPECT_TRUE(preview.problems.empty()) << (preview.problems.empty() ? "" : preview.problems.front());

    /// the same files, and the same size, as the compile's report has afterwards
    const compile_report report = compile(settings());
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    EXPECT_EQ(preview.left_out.size(), 5);
    EXPECT_EQ(preview.left_out, report.left_out);
    EXPECT_EQ(preview.left_out_bytes, report.left_out_bytes);

    /// a preview by itself makes nothing, and follows [package] keep as a compile does
    std::filesystem::remove_all(output);
    write("game/game.toml", std::format("{}\n[package]\nkeep = [ \"assets/audio/**\" ]\n", kProject));
    preview = preview_project_compile(bus, project_folder);
    EXPECT_EQ(preview.left_out.size(), 4);
    EXPECT_FALSE(std::ranges::contains(preview.left_out, std::string{ "assets/audio/voice/line-1.wav" }));
    EXPECT_TRUE(nothing_was_written());
    EXPECT_FALSE(std::filesystem::exists(project_folder / "boot.obin"));
  }

  TEST_F(project_compiler_tests, a_preview_says_what_stops_the_listing_and_lists_nothing_then) {
    write_content();
    command_bus bus;
    register_builtin_command_devices(bus);

    write("game/game.toml", std::format("{}\n[package]\nkeep = [ \"assets/**.png\" ]\n", kProject));
    compile_preview preview = preview_project_compile(bus, project_folder);
    EXPECT_TRUE(any_holds(preview.problems, "[package] keep has the pattern 'assets/**.png'"));
    EXPECT_TRUE(preview.left_out.empty());

    write("game/game.toml", kProject);
    write("game/assets/scenes/second.oscn", "[scene\nthis is not a scene");
    preview = preview_project_compile(bus, project_folder);
    EXPECT_TRUE(any_holds(preview.problems, "scene 'Second' ('game/assets/scenes/second.oscn') cannot be read"));
    EXPECT_TRUE(preview.left_out.empty());
    /// and it still says what it read before that
    EXPECT_EQ(preview.scenes.size(), 2);
  }

  TEST_F(project_compiler_tests, a_compile_says_where_it_is_and_ends_at_its_last_stage) {
    compile_progress progress;
    compile_settings watched = settings();
    watched.progress = &progress;
    const compile_report report = compile(watched);
    ASSERT_TRUE(report.success) << (report.errors.empty() ? "" : report.errors.front());
    EXPECT_FALSE(report.cancelled);
    EXPECT_EQ(progress.stage.load(), compile_stage::FINISH);

    /// every stage has words for a person, and no two have the same
    ostd::vector<std::string_view> labels;
    for (uint32_t stage = 0; stage < static_cast<uint32_t>(compile_stage::NUM_STAGES); ++stage) {
      const std::string_view label = compile_stage_label(static_cast<compile_stage>(stage));
      EXPECT_FALSE(label.empty());
      EXPECT_FALSE(std::ranges::contains(labels, label)) << label;
      labels.push_back(label);
    }
  }

  TEST_F(project_compiler_tests, a_cancelled_compile_makes_nothing_and_leaves_the_last_package_alone) {
    ASSERT_TRUE(compile(settings()).success);
    const filepath package = output / "Sandbox Game";
    const auto made_at = std::filesystem::last_write_time(package / "Sandbox Game.toml");

    compile_progress progress;
    progress.cancel_requested = true;
    compile_settings watched = settings();
    watched.progress = &progress;
    const compile_report report = compile(watched);
    EXPECT_FALSE(report.success);
    EXPECT_TRUE(report.cancelled);
    /// being told to stop is not an error
    EXPECT_TRUE(report.errors.empty());
    EXPECT_EQ(std::filesystem::last_write_time(package / "Sandbox Game.toml"), made_at);
    EXPECT_FALSE(std::filesystem::exists(output / ".staging"));
  }

  /// the same project, compiled through a session on a real job system
  class compile_session_tests : public project_compiler_tests {
   protected:
    void SetUp() override {
      project_compiler_tests::SetUp();
      register_builtin_command_devices(bus);
      jobs = std::make_unique<job_system>(io);
      jobs->initialize(config_table::load_from_source("[application.async]\nworker_count = 2\n"));
    }

    void TearDown() override {
      jobs->shutdown();
      jobs = nullptr;
      project_compiler_tests::TearDown();
    }

    /// polls as an owner with a frame to draw does; false when the compile never finishes
    bool poll_until_finished(compile_session& session) {
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
      while (std::chrono::steady_clock::now() < deadline) {
        jobs->poll();
        if (session.poll()) {
          return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      return false;
    }

    command_bus bus;
    std::unique_ptr<job_system> jobs;
  };

  TEST_F(compile_session_tests, a_compile_runs_on_a_worker_and_its_report_is_there_when_polling_finds_it_done) {
    compile_session session;
    EXPECT_FALSE(session.running());
    EXPECT_FALSE(session.poll());
    EXPECT_FALSE(session.last_report().has_value());

    ASSERT_TRUE(session.start(*jobs, bus, settings()));
    EXPECT_TRUE(session.running());
    EXPECT_FALSE(session.last_report().has_value());

    ASSERT_TRUE(poll_until_finished(session));
    EXPECT_FALSE(session.running());
    ASSERT_TRUE(session.last_report().has_value());
    EXPECT_TRUE(session.last_report()->success) << (session.last_report()->errors.empty() ? "" : session.last_report()->errors.front());
    EXPECT_EQ(session.last_report()->package_root, output / "Sandbox Game");
    EXPECT_EQ(read(output / "Sandbox Game" / "Sandbox Game.exe"), "the runtime");
    /// finished is found once
    EXPECT_FALSE(session.poll());
  }

  TEST_F(compile_session_tests, one_compile_at_a_time_and_the_next_one_clears_the_last_report) {
    compile_session session;
    ASSERT_TRUE(session.start(*jobs, bus, settings()));
    /// refused however far the first one has got: it is running until a poll has taken its report
    EXPECT_FALSE(session.start(*jobs, bus, settings(package_config::DEBUG)));
    session.wait();
    ASSERT_TRUE(session.last_report().has_value());
    EXPECT_TRUE(session.last_report()->success);
    EXPECT_FALSE(std::filesystem::exists(output / "Sandbox Game-debug"));

    ASSERT_TRUE(session.start(*jobs, bus, settings(package_config::DEBUG)));
    EXPECT_FALSE(session.last_report().has_value());
    session.wait();
    ASSERT_TRUE(session.last_report().has_value());
    EXPECT_TRUE(session.last_report()->success);
    EXPECT_TRUE(std::filesystem::exists(output / "Sandbox Game-debug" / "Sandbox Game.exe"));
  }

  TEST_F(compile_session_tests, a_compile_that_fails_reports_why) {
    compile_settings named = settings();
    named.public_name = "What?";
    compile_session session;
    ASSERT_TRUE(session.start(*jobs, bus, named));
    /// waiting needs nobody to poll the job system
    session.wait();
    EXPECT_FALSE(session.running());
    ASSERT_TRUE(session.last_report().has_value());
    EXPECT_FALSE(session.last_report()->success);
    EXPECT_TRUE(any_holds(session.last_report()->errors, "the public name 'What?' cannot be used"));
    EXPECT_TRUE(nothing_was_written());
  }

  TEST_F(compile_session_tests, a_running_compile_can_be_cancelled_and_the_next_one_is_not) {
    compile_session session;
    ASSERT_TRUE(session.start(*jobs, bus, settings()));
    EXPECT_FALSE(session.cancelling());
    session.cancel();
    EXPECT_TRUE(session.cancelling());
    session.wait();
    EXPECT_FALSE(session.cancelling());
    ASSERT_TRUE(session.last_report().has_value());
    EXPECT_TRUE(session.last_report()->cancelled);
    EXPECT_FALSE(session.last_report()->success);
    EXPECT_FALSE(std::filesystem::exists(output / "Sandbox Game"));
    EXPECT_FALSE(std::filesystem::exists(output / ".staging"));

    /// a cancel belongs to the compile it was asked of
    ASSERT_TRUE(session.start(*jobs, bus, settings()));
    EXPECT_GE(session.fraction(), 0.f);
    EXPECT_LT(session.fraction(), 1.f);
    EXPECT_FALSE(session.stage_label().empty());
    session.wait();
    ASSERT_TRUE(session.last_report().has_value());
    EXPECT_TRUE(session.last_report()->success);
    EXPECT_FALSE(session.last_report()->cancelled);
  }

  TEST_F(compile_session_tests, a_session_that_goes_away_waits_for_its_compile) {
    {
      compile_session session;
      ASSERT_TRUE(session.start(*jobs, bus, settings()));
    }
    /// the worker wrote into the session until it had finished, so the package is whole
    EXPECT_TRUE(std::filesystem::exists(output / "Sandbox Game" / kPackageMarker));
    EXPECT_FALSE(std::filesystem::exists(output / ".staging"));
  }

}  // namespace other
