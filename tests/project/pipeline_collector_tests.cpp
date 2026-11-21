/**
 * \file tests/project/pipeline_collector_tests.cpp
 *
 * a project's [rendering] table, the files its pipelines need, and the order they load in.
 **/
#include <filesystem>
#include <fstream>
#include <sstream>

#include "file/path_helpers.hpp"

#include "project/pipeline_collector.hpp"

#include "other_test.hpp"

namespace other {

  class pipeline_collector_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-pipeline-collector-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      project_folder = sandbox / "game";
      project_file = project_folder / "game.toml";
      write("game.toml", "");
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
      other_test::TearDown();
    }

    /// path is from the project's folder
    void write(const filepath& path, const std::string_view text) {
      const filepath full = project_folder / path;
      std::filesystem::create_directories(full.parent_path());
      std::ofstream out(full, std::ios::binary | std::ios::trunc);
      ASSERT_TRUE(out.is_open()) << full.string();
      out.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    static filepath default_pipeline() {
      return resolve_asset_path(kDefaultProjectPipeline).resolved;
    }

    /// the engine's default pipeline under another name, with changes made to its text. a
    ///  pipeline has a lot that must be right; this keeps all of it and alters only what a test is about
    void write_pipeline(const filepath& path, const std::string_view name, std::initializer_list<std::pair<std::string_view, std::string_view>> changes = {}) {
      std::ifstream in(default_pipeline(), std::ios::binary);
      ASSERT_TRUE(in.is_open());
      std::stringstream buffer;
      buffer << in.rdbuf();
      std::string text = buffer.str();

      const auto replace = [&text](const std::string_view old_text, const std::string_view new_text) {
        const size_t at = text.find(old_text);
        ASSERT_NE(at, std::string::npos) << old_text;
        text.replace(at, old_text.size(), new_text);
      };
      replace("name = \"default-instancing\"", std::format("name = \"{}\"", name));
      for (const auto& [old_text, new_text] : changes) {
        replace(old_text, new_text);
      }
      write(path, text);
    }

    static bool lists(std::span<const filepath> files, const filepath& file) {
      return std::ranges::find(files, file.lexically_normal()) != files.end();
    }

    static bool any_holds(std::span<const std::string> errors, const std::string_view part) {
      return std::ranges::any_of(errors, [part](const std::string& error) { return error.contains(part); });
    }

    static ostd::vector<std::string> names_of(std::span<const collected_pipeline> pipelines) {
      ostd::vector<std::string> names;
      for (const collected_pipeline& pipeline : pipelines) {
        names.push_back(pipeline.name);
      }
      return names;
    }

    filepath sandbox;
    filepath project_folder;
    filepath project_file;
  };

  TEST_F(pipeline_collector_tests, a_project_with_no_rendering_table_gets_the_engine_default) {
    const opt<project_rendering_settings> settings = read_project_rendering_settings(project_file);
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->pipelines, (ostd::vector<std::string>{ std::string{ kDefaultProjectPipeline } }));
    EXPECT_FALSE(settings->main_pipeline.has_value());

    /// and the default is a file the engine has
    ostd::vector<std::string> errors;
    const ostd::vector<filepath> files = project_pipeline_files(*settings, errors);
    EXPECT_TRUE(errors.empty());
    ASSERT_EQ(files.size(), 1);
    EXPECT_TRUE(std::filesystem::exists(files.front()));
  }

  TEST_F(pipeline_collector_tests, the_rendering_table_is_read_as_it_is_written) {
    write("game.toml", "[rendering]\nmain-pipeline = \"mine\"\npipelines = [ \"assets/pipelines/mine.toml\", \"engine://resources/editor-assets/default-pipeline.toml\" ]\n");

    const opt<project_rendering_settings> settings = read_project_rendering_settings(project_file);
    ASSERT_TRUE(settings.has_value());
    EXPECT_EQ(settings->pipelines, (ostd::vector<std::string>{ "assets/pipelines/mine.toml", "engine://resources/editor-assets/default-pipeline.toml" }));
    EXPECT_EQ(settings->main_pipeline, "mine");
  }

  TEST_F(pipeline_collector_tests, a_project_file_that_cannot_be_read_gives_no_settings) {
    write("game.toml", "[rendering\npipelines = ");
    EXPECT_FALSE(read_project_rendering_settings(project_file).has_value());
    EXPECT_FALSE(read_project_rendering_settings(project_folder / "missing.toml").has_value());
  }

  TEST_F(pipeline_collector_tests, an_entry_is_found_the_way_any_asset_path_is) {
    /// beside the project file, through a mount, from the working directory, and not at all
    write("game.toml",
          "[rendering]\npipelines = [ \"assets/pipelines/mine.toml\", \"engine://resources/editor-assets/default-pipeline.toml\", "
          "\"resources/editor-assets/test-pipeline.toml\", \"assets/pipelines/gone.toml\" ]\n");
    write_pipeline("assets/pipelines/mine.toml", "mine");

    const opt<project_rendering_settings> settings = read_project_rendering_settings(project_file);
    ASSERT_TRUE(settings.has_value());
    ostd::vector<std::string> errors;
    const ostd::vector<filepath> files = project_pipeline_files(*settings, errors);

    ASSERT_EQ(files.size(), 3);
    EXPECT_EQ(files[0], (project_folder / "assets/pipelines/mine.toml").lexically_normal());
    EXPECT_EQ(files[1], default_pipeline());
    EXPECT_EQ(files[2], (std::filesystem::current_path() / "resources/editor-assets/test-pipeline.toml").lexically_normal());
    ASSERT_EQ(errors.size(), 1);
    EXPECT_TRUE(any_holds(errors, "'assets/pipelines/gone.toml' in 'game.toml' does not exist"));
  }

  TEST_F(pipeline_collector_tests, the_engine_default_collects_with_every_file_it_needs) {
    const filepath root = default_pipeline();
    const pipeline_collection collection = collect_pipelines(std::span{ &root, 1 });
    ASSERT_TRUE(collection.success) << (collection.errors.empty() ? "" : collection.errors.front());
    ASSERT_EQ(collection.pipelines.size(), 1);

    const collected_pipeline& pipeline = collection.pipelines.front();
    EXPECT_EQ(pipeline.name, "default-instancing");
    EXPECT_EQ(pipeline.source, root);
    EXPECT_TRUE(pipeline.depends_on.empty());

    /// a shader stage, a file only an include reaches, and a seed texture
    const filepath resources = environment_root() / "resources";
    EXPECT_TRUE(lists(pipeline.files, resources / "basic-shading.frag"));
    EXPECT_TRUE(lists(pipeline.files, resources / "shader-modules/camera.glsl"));
    EXPECT_TRUE(lists(pipeline.files, resources / "smaa-area-tex.bin"));

    EXPECT_TRUE(std::ranges::is_sorted(pipeline.files));
    EXPECT_EQ(std::ranges::adjacent_find(pipeline.files), pipeline.files.end());
    for (const filepath& file : pipeline.files) {
      EXPECT_TRUE(std::filesystem::is_regular_file(file)) << file.string();
    }
  }

  TEST_F(pipeline_collector_tests, a_projects_own_pipeline_collects_its_shaders_their_includes_and_its_seeds) {
    write_pipeline("assets/pipelines/mine.toml", "mine",
                   {
                     { "engine://resources/basic-tonemap.frag", "shaders/tonemap.frag" },
                     { "engine://resources/smaa-search-tex.bin", "textures/search.bin" },
                   });
    /// an include is looked for in the folder of the shader that was opened first, however deep it is
    write("assets/pipelines/shaders/tonemap.frag", "#include \"parts/curve.glsl\"\nvoid main() {}\n");
    write("assets/pipelines/shaders/parts/curve.glsl", "  #  include \"parts/constants.glsl\"\n");
    /// the loader takes a line as an include only when '#' starts it, so these two are not
    write("assets/pipelines/shaders/parts/constants.glsl", "// #include \"parts/unused.glsl\"\n#include\"parts/unused.glsl\"\n");
    write("assets/pipelines/shaders/parts/unused.glsl", "");
    write("assets/pipelines/textures/search.bin", "seed");

    const filepath root = project_folder / "assets/pipelines/mine.toml";
    const pipeline_collection collection = collect_pipelines(std::span{ &root, 1 });
    ASSERT_TRUE(collection.success) << (collection.errors.empty() ? "" : collection.errors.front());
    ASSERT_EQ(collection.pipelines.size(), 1);

    const collected_pipeline& pipeline = collection.pipelines.front();
    EXPECT_EQ(pipeline.name, "mine");
    const filepath shaders = project_folder / "assets/pipelines/shaders";
    EXPECT_TRUE(lists(pipeline.files, shaders / "tonemap.frag"));
    EXPECT_TRUE(lists(pipeline.files, shaders / "parts/curve.glsl"));
    EXPECT_TRUE(lists(pipeline.files, shaders / "parts/constants.glsl"));
    EXPECT_FALSE(lists(pipeline.files, shaders / "parts/unused.glsl"));
    EXPECT_TRUE(lists(pipeline.files, project_folder / "assets/pipelines/textures/search.bin"));
    /// what was replaced is not collected, and the engine's other shaders still are
    EXPECT_FALSE(lists(pipeline.files, environment_root() / "resources/basic-tonemap.frag"));
    EXPECT_TRUE(lists(pipeline.files, environment_root() / "resources/basic-shading.frag"));
  }

  TEST_F(pipeline_collector_tests, a_file_a_pipeline_needs_and_does_not_have_is_an_error) {
    write_pipeline("assets/pipelines/no-shader.toml", "no-shader", { { "engine://resources/basic-tonemap.frag", "shaders/gone.frag" } });
    write_pipeline("assets/pipelines/no-include.toml", "no-include", { { "engine://resources/basic-tonemap.frag", "shaders/tonemap.frag" } });
    write_pipeline("assets/pipelines/no-seed.toml", "no-seed", { { "engine://resources/smaa-search-tex.bin", "textures/gone.bin" } });
    write("assets/pipelines/shaders/tonemap.frag", "#include \"parts/gone.glsl\"\n");

    filepath root = project_folder / "assets/pipelines/no-shader.toml";
    pipeline_collection collection = collect_pipelines(std::span{ &root, 1 });
    EXPECT_FALSE(collection.success);
    EXPECT_TRUE(collection.pipelines.empty());
    EXPECT_TRUE(any_holds(collection.errors, "pipeline 'no-shader': shader 'tonemap_pass_shader' file 'shaders/gone.frag' was not found"));

    root = project_folder / "assets/pipelines/no-include.toml";
    collection = collect_pipelines(std::span{ &root, 1 });
    EXPECT_FALSE(collection.success);
    EXPECT_TRUE(any_holds(collection.errors, "pipeline 'no-include': 'tonemap.frag' includes 'parts/gone.glsl'"));

    root = project_folder / "assets/pipelines/no-seed.toml";
    collection = collect_pipelines(std::span{ &root, 1 });
    EXPECT_FALSE(collection.success);
    EXPECT_TRUE(any_holds(collection.errors, "pipeline 'no-seed': seed for texture 'smaa_search_texture' 'textures/gone.bin' was not found"));
  }

  TEST_F(pipeline_collector_tests, what_is_not_a_pipeline_is_an_error_and_not_an_assert) {
    write("assets/pipelines/scene.toml", "asset-type = \"scene\"\nname = \"x\"\nversion = 1\n");
    write("assets/pipelines/nameless.toml", "asset-type = \"rendering-pipeline\"\nversion = 1\n");
    write("assets/pipelines/broken.toml", "asset-type = ");
    write("assets/pipelines/pipeline.txt", "asset-type = \"rendering-pipeline\"\n");

    for (const std::string_view name : { "scene.toml", "nameless.toml", "broken.toml", "pipeline.txt", "missing.toml" }) {
      const filepath root = project_folder / "assets/pipelines" / name;
      const pipeline_collection collection = collect_pipelines(std::span{ &root, 1 });
      EXPECT_FALSE(collection.success) << name;
      EXPECT_EQ(collection.errors.size(), 1) << name;
    }
  }

  TEST_F(pipeline_collector_tests, two_pipelines_of_one_name_are_an_error_and_one_file_listed_twice_is_not) {
    write_pipeline("assets/pipelines/one.toml", "same");
    write_pipeline("assets/pipelines/two.toml", "same");

    const filepath one = project_folder / "assets/pipelines/one.toml";
    const filepath two = project_folder / "assets/pipelines/two.toml";
    const filepath clash[] = { one, two };
    const pipeline_collection clashing = collect_pipelines(clash);
    EXPECT_FALSE(clashing.success);
    EXPECT_TRUE(any_holds(clashing.errors, "two pipelines are named 'same'"));

    const filepath twice[] = { one, project_folder / "assets/pipelines/../pipelines/one.toml" };
    const pipeline_collection listed_twice = collect_pipelines(twice);
    EXPECT_TRUE(listed_twice.success);
    EXPECT_EQ(listed_twice.pipelines.size(), 1);
  }

  TEST_F(pipeline_collector_tests, several_pipelines_keep_the_order_they_were_given_in) {
    write_pipeline("assets/pipelines/b.toml", "second");
    const filepath roots[] = { default_pipeline(), project_folder / "assets/pipelines/b.toml" };
    const pipeline_collection collection = collect_pipelines(roots);
    ASSERT_TRUE(collection.success);
    EXPECT_EQ(names_of(collection.pipelines), (ostd::vector<std::string>{ "default-instancing", "second" }));
  }

  TEST_F(pipeline_collector_tests, a_pipeline_is_ordered_after_what_it_depends_on) {
    /// no pipeline file can declare a dependency yet, so the edges are made here
    ostd::vector<collected_pipeline> pipelines = {
      { .name = "post", .depends_on = { "lighting" } },
      { .name = "lighting", .depends_on = { "geometry", "shadows" } },
      { .name = "ui" },
      { .name = "geometry" },
      { .name = "shadows", .depends_on = { "geometry" } },
    };
    ostd::vector<std::string> errors;
    ASSERT_TRUE(order_pipelines_by_dependency(pipelines, errors));
    EXPECT_TRUE(errors.empty());
    EXPECT_EQ(names_of(pipelines), (ostd::vector<std::string>{ "geometry", "shadows", "lighting", "post", "ui" }));
  }

  TEST_F(pipeline_collector_tests, a_circle_of_dependencies_is_an_error_that_names_the_circle) {
    ostd::vector<collected_pipeline> pipelines = {
      { .name = "first", .depends_on = { "second" } },
      { .name = "second", .depends_on = { "third" } },
      { .name = "third", .depends_on = { "first" } },
    };
    ostd::vector<std::string> errors;
    EXPECT_FALSE(order_pipelines_by_dependency(pipelines, errors));
    EXPECT_TRUE(any_holds(errors, "first -> second -> third -> first"));
    /// left as it was given
    EXPECT_EQ(names_of(pipelines), (ostd::vector<std::string>{ "first", "second", "third" }));
  }

  TEST_F(pipeline_collector_tests, a_dependency_that_is_not_being_collected_is_an_error) {
    ostd::vector<collected_pipeline> pipelines = {
      { .name = "post", .depends_on = { "lighting" } },
    };
    ostd::vector<std::string> errors;
    EXPECT_FALSE(order_pipelines_by_dependency(pipelines, errors));
    EXPECT_TRUE(any_holds(errors, "pipeline 'post' needs 'lighting'"));
  }

}  // namespace other
