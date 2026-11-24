/**
 * \file project/project_compiler.cpp
 **/
#include "project/project_compiler.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

#include "core/logger.hpp"
#include "core/profiler.hpp"
#include "file/filesystem.hpp"
#include "file/glob.hpp"
#include "file/path_helpers.hpp"
#include "serialization/scene_serializer.hpp"

#include "model/model_importer.hpp"

#include "project/pipeline_collector.hpp"
#include "project/project.hpp"
#include "project/script_assembly.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/project_program.hpp"

#include "asset/asset_resolver.hpp"

namespace other {
  namespace {

    constexpr std::string_view kStagingFolder = ".staging";
    constexpr std::string_view kBootProgram = "boot.obin";
    /// what a project folder holds that a package must not: build output, logs, source control
    constexpr std::array<std::string_view, 4> kSkippedProjectFolders = { "bin", "obj", "logs", ".git" };
    constexpr std::array<std::string_view, 22> kReservedFileNames = {
      "CON",
      "PRN",
      "AUX",
      "NUL",
      "COM1",
      "COM2",
      "COM3",
      "COM4",
      "COM5",
      "COM6",
      "COM7",
      "COM8",
      "COM9",
      "LPT1",
      "LPT2",
      "LPT3",
      "LPT4",
      "LPT5",
      "LPT6",
      "LPT7",
      "LPT8",
      "LPT9",
    };

    opt<std::string> read_text(const filepath& path) {
      std::ifstream file(path, std::ios::binary);
      if (!file.is_open()) {
        return std::nullopt;
      }
      std::ostringstream text;
      text << file.rdbuf();
      return text.str();
    }

    bool write_text(const filepath& path, const std::string_view text) {
      std::error_code ec;
      std::filesystem::create_directories(path.parent_path(), ec);
      std::ofstream file(path, std::ios::binary | std::ios::trunc);
      if (!file.is_open()) {
        return false;
      }
      file.write(text.data(), static_cast<std::streamsize>(text.size()));
      return file.good();
    }

    /// the path from root to file, when file is inside root
    opt<filepath> inside(const filepath& root, const filepath& file) {
      std::error_code ec;
      const filepath relative = std::filesystem::relative(file, root, ec);
      if (ec || relative.empty() || relative.is_absolute() || *relative.begin() == ".." || relative == ".") {
        return std::nullopt;
      }
      return relative;
    }

    std::string toml_string(const std::string_view value) {
      std::string out = "\"";
      for (const char c : value) {
        if (c == '\\' || c == '"') {
          out += '\\';
        }
        out += c;
      }
      out += '"';
      return out;
    }

    std::string_view trimmed(std::string_view line) {
      const auto is_blank = [](const char c) -> bool {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
      };
      while (!line.empty() && is_blank(line.front())) {
        line.remove_prefix(1);
      }
      while (!line.empty() && is_blank(line.back())) {
        line.remove_suffix(1);
      }
      return line;
    }

    /// each line with its own ending kept, so putting them back together changes nothing
    ostd::vector<std::string> split_lines(std::string_view text) {
      ostd::vector<std::string> lines;
      while (!text.empty()) {
        const size_t end = text.find('\n');
        const size_t count = end == std::string_view::npos ? text.size() : end + 1;
        lines.emplace_back(text.substr(0, count));
        text.remove_prefix(count);
      }
      return lines;
    }

    std::string joined(std::span<const std::string> lines) {
      std::string text;
      for (const std::string& line : lines) {
        text += line;
      }
      return text;
    }

    bool is_table_header(const std::string_view line, const std::string_view table) {
      const std::string_view text = trimmed(line);
      const std::string header = std::format("[{}]", table);
      if (!text.starts_with(header)) {
        return false;
      }
      /// only a comment may follow on the line
      const std::string_view rest = trimmed(text.substr(header.size()));
      return rest.empty() || rest.starts_with('#');
    }

    bool is_key_line(const std::string_view line, const std::string_view key) {
      std::string_view text = trimmed(line);
      if (!text.starts_with(key)) {
        return false;
      }
      text = trimmed(text.substr(key.size()));
      return text.starts_with('=');
    }

    struct table_position {
      /// lines.size() when the table is not in the text
      size_t header = 0;
      /// the key's line inside the table, when it has one
      opt<size_t> key;
    };

    table_position find_in_table(std::span<const std::string> lines, const std::string_view table, const std::string_view key) {
      table_position position = { .header = lines.size() };
      for (size_t i = 0; i < lines.size(); ++i) {
        if (is_table_header(lines[i], table)) {
          position.header = i;
          break;
        }
      }
      /// the table runs to the next header of any kind
      for (size_t i = position.header + 1; i < lines.size() && !trimmed(lines[i]).starts_with('['); ++i) {
        if (is_key_line(lines[i], key)) {
          position.key = i;
          break;
        }
      }
      return position;
    }

    std::string_view line_ending_of(const std::string_view text) {
      return text.contains("\r\n") ? "\r\n" : "\n";
    }

    /// what a compile needs of the process for its own run, put back when it ends.
    ///  the mounts: the engine's, and the project's under its folder's name, as a project load
    ///  makes it. one that is already there (a compile run from inside the editor) is left alone.
    ///  the working directory: a project's paths, and the asset ids in its scenes, are found
    ///  from the folder that holds the project's folder, which is where the packaged game runs
    class scoped_project_environment {
     public:
      scoped_project_environment(const filepath& engine_root, const std::string& project_name, const filepath& project_folder) {
        /// left alone when it is already there, which it is in the editor: the working directory
        ///  belongs to the whole process, and a compile may be on a worker thread
        std::error_code ec;
        const filepath current = std::filesystem::current_path(ec);
        if (!ec && !std::filesystem::equivalent(current, project_folder.parent_path(), ec)) {
          previous_directory = current;
          std::filesystem::current_path(project_folder.parent_path(), ec);
        }
        add(file_system::kEngineMount, engine_root, mount_scope::ENGINE);
        add(project_name, project_folder, mount_scope::PROJECT);
      }
      ~scoped_project_environment() {
        auto* fs = subsystem<file_system>::get();
        for (const std::string& name : added) {
          fs->unmount(name);
        }
        if (!previous_directory.empty()) {
          std::error_code ec;
          std::filesystem::current_path(previous_directory, ec);
        }
      }

      scoped_project_environment(const scoped_project_environment&) = delete;
      scoped_project_environment& operator=(const scoped_project_environment&) = delete;

     private:
      ostd::vector<std::string> added;
      /// empty when the working directory was not changed
      filepath previous_directory;

      void add(const std::string_view name, const filepath& folder, const mount_scope scope) {
        auto* fs = subsystem<file_system>::get();
        OTHER_ASSERT(fs != nullptr, "file_system subsystem is not available for a project compile");
        if (fs->get_mount(name) == nullptr && fs->mount_directory(name, folder, scope) != nullptr) {
          added.emplace_back(name);
        }
      }
    };

    /// the kinds of file a scene reaches through its objects: models, textures, materials,
    ///  audio, animation clips and other scenes. only these can be told to be unused. anything
    ///  else a project holds may be read by a script, so it is always copied
    bool is_scene_content(const filepath& file) {
      switch (asset::get_type_from_extension(file.extension().string())) {
        case asset::TEXTURE:
        case asset::MODEL_SOURCE:
        case asset::ANIMATION:
        case asset::AUDIO:
        case asset::MATERIAL:
        case asset::SCENE: return true;
        default: return false;
      }
    }

    /// one spelling for a path inside the project, to look a file up by
    std::string file_key(const filepath& relative) {
      std::string key = relative.generic_string();
#if OTHER_ENVIRONMENT_WINDOWS
      std::ranges::transform(key, key.begin(), [](const char c) -> char { return detail::fold_case(c); });
#endif
      return key;
    }

    /// glob_set asserts on a pattern it cannot read, and these are written by hand
    opt<std::string> invalid_pattern_reason(const std::string_view pattern) {
      if (pattern.empty()) {
        return "it is empty";
      }
      if (pattern.contains('\\')) {
        return "folders are separated by '/'";
      }
      if (pattern.front() == '/' || pattern.back() == '/') {
        return "it is a path from the project's folder, with no '/' at either end";
      }
      if (pattern.find_first_of("{}[]") != std::string_view::npos) {
        return "braces and brackets are not supported";
      }
      for (std::string_view rest = pattern; !rest.empty();) {
        const size_t slash = rest.find('/');
        const std::string_view segment = rest.substr(0, slash);
        if (segment.empty()) {
          return "it has an empty folder name";
        }
        if (segment.contains("**") && segment != "**") {
          return "'**' has to be a whole folder name";
        }
        rest = slash == std::string_view::npos ? std::string_view{} : rest.substr(slash + 1);
      }
      return std::nullopt;
    }

    struct scene_entry {
      std::string name;
      /// the path as the project file has it, which is what is replaced in the packaged file
      std::string written;
      filepath file;
      std::string source_key;
      /// where the binary copy goes, from the project's folder
      filepath packaged;
      serialization::scene_document document;
    };

    /// a scene's path as the project file writes it, pointed at the binary copy
    std::string packaged_scene_path(const std::string_view written) {
      return std::format("{}{}", written.substr(0, written.rfind('.')), serialization::kSceneBinaryExtension);
    }

    /// a game's name when nobody chose one: the project's own key, then the name in its
    ///  metadata, then its folder
    std::string default_public_name(const toml::table& table, const std::string_view project_name) {
      std::string name{ project_name };
      if (const toml::array* metadata = table.at_path("project.metadata").as_array(); metadata != nullptr) {
        for (const toml::node& entry : *metadata) {
          const toml::table* row = entry.as_table();
          if (row != nullptr && row->at_path("key").value_or(std::string{}) == "name") {
            name = row->at_path("value").value_or(name);
          }
        }
      }
      return table.at_path("project.public-name").value_or(name);
    }

    struct plugin_entry {
      std::string name;
      /// the path as the project file has it, which is what is replaced in the packaged file
      std::string written;
      /// the library to ship: the one built in the package's configuration
      filepath file;
    };

    /// one compile, from the settings to the swapped-in package. each stage returns false
    ///  once it has put its reason in the report
    struct compile_run {
      const command_bus& bus;
      const compile_settings& settings;
      compile_report report = {};
      /// for a preview: every check and every count as a compile makes them, and nothing is
      ///  copied, chosen or said in the log
      bool listing_only = false;

      engine_sources engine = {};
      filepath project_file;
      filepath project_folder;
      std::string project_name;
      /// the folder that holds the project's folder. a project's own paths are written from
      ///  here, and a package has the same shape, so they still hold inside it
      filepath base;
      std::string project_text;
      toml::table table;
      bool debug = false;

      std::string public_name;
      filepath output;
      filepath final_folder;
      filepath staging;
      filepath staged_project;

      ostd::vector<scene_entry> scenes;
      std::string starting_scene;
      /// the project's VM sources, as paths from its folder; they are built in and not copied
      ostd::vector<std::string> vm_sources;
      /// as the packaged project file names it; empty when the project has no scripts
      std::string packaged_assembly;
      ostd::vector<plugin_entry> plugins;
      ostd::vector<std::string> config_pipelines;
      std::string main_pipeline;
      /// engine files the package needs, absolute
      ostd::vector<filepath> engine_files;
      /// the project's files that something in the package uses, as file keys
      ostd::vector<std::string> used;
      /// [package] keep: what ships although nothing a compile can see uses it
      glob_set kept;
      /// [package] exclude: what never ships
      glob_set excluded;

      bool fail(std::string reason) {
        CORE_LOG_ERROR("[COMPILE] : {}", reason);
        report.errors.push_back(std::move(reason));
        return false;
      }

      void warn(std::string reason) {
        if (!listing_only) {
          CORE_LOG_WARN("[COMPILE] : {}", reason);
        }
        report.warnings.push_back(std::move(reason));
      }

      /// true once whoever watches has asked for the compile to stop. said once
      bool cancelled() {
        if (settings.progress == nullptr || !settings.progress->cancel_requested.load()) {
          return false;
        }
        if (!report.cancelled) {
          report.cancelled = true;
          CORE_LOG_INFO("[COMPILE] : cancelled while {}", compile_stage_label(settings.progress->stage.load()));
        }
        return true;
      }

      /// the start of a stage: says where the compile is, and is false when it was cancelled
      bool enter(const compile_stage stage) {
        if (settings.progress != nullptr) {
          settings.progress->stage = stage;
        }
        return !cancelled();
      }

      bool copy(const filepath& from, const filepath& to) {
        if (listing_only) {
          return true;
        }
        if (cancelled()) {
          return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(to.parent_path(), ec);
        std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
        return ec ? fail(std::format("could not copy '{}' to '{}': {}", from.string(), to.string(), ec.message())) : true;
      }

      /// a path written in the project file: through its mount when it names one, and
      ///  otherwise from the folder the project's paths are written from. empty when a
      ///  mount path leads nowhere
      filepath project_path(const std::string_view written) const {
        const std::string text = perform_tag_replacement(written);
        if (text.contains(file_system::kPathSeparator)) {
          return resolve_asset_path(text, project_file).resolved;
        }
        const filepath as_written{ text };
        return as_written.is_absolute() ? as_written : (base / as_written).lexically_normal();
      }

      /// how a file is named in the package's runtime config; empty when it is in neither
      ///  the project nor the engine's resources and so is not in the package
      std::string packaged_entry(const filepath& file) const {
        if (const opt<filepath> in_project = inside(project_folder, file); in_project.has_value()) {
          return std::format("{}/{}", project_name, in_project->generic_string());
        }
        if (const opt<filepath> in_engine = inside(engine.root / "resources", file); in_engine.has_value()) {
          return std::format("engine://resources/{}", in_engine->generic_string());
        }
        return {};
      }

      /// a file something in the package needs: the project's is marked as used, the engine's
      ///  is copied from the engine, and one that is in neither cannot come along
      void need(const filepath& file, const std::string_view needed_by) {
        if (const opt<filepath> in_project = inside(project_folder, file); in_project.has_value()) {
          used.push_back(file_key(*in_project));
        } else if (inside(engine.root / "resources", file).has_value()) {
          engine_files.push_back(file);
        } else {
          warn(std::format("{} uses '{}', which is in neither the project nor the engine's resources; it is not in the package", needed_by, file.string()));
        }
      }

      /// an asset id as a packaged scene has to have it. an editor used to save an engine
      ///  file's id through the working directory ('cwd://...'), which holds only where that
      ///  editor was started; a package has the engine's files under engine/. such an id is
      ///  given the mount of the file it leads to
      std::string packaged_asset_id(const std::string& id, const std::string_view scene_name) {
        const std::string through_working_directory = std::format("{}{}", file_system::kCwd, file_system::kPathSeparator);
        if (!id.starts_with(through_working_directory)) {
          return id;
        }
        /// that editor's working directory was the folder that holds the project's folder,
        ///  and in a source tree that is the engine's root as well. no mount is asked: a
        ///  command-line tool has none for the working directory
        const filepath written{ id.substr(through_working_directory.size()) };
        std::error_code ec;
        for (const filepath& root : { base, engine.root }) {
          const filepath file = (root / written).lexically_normal();
          if (!std::filesystem::is_regular_file(file, ec)) {
            continue;
          }
          if (const opt<filepath> in_project = inside(project_folder, file); in_project.has_value()) {
            return std::format("{}{}{}", project_name, file_system::kPathSeparator, in_project->generic_string());
          }
          if (const opt<filepath> in_engine = inside(engine.root, file); in_engine.has_value()) {
            return std::format("{}{}{}", file_system::kEngineMount, file_system::kPathSeparator, in_engine->generic_string());
          }
        }
        warn(std::format("scene '{}' names '{}' through the working directory, which leads to no file of the project or the engine; the packaged game will not find it", scene_name, id));
        return id;
      }

      bool read_project();
      bool read_package_rules();
      bool check_names_and_output();
      bool check_scenes();
      bool check_scripts();
      bool check_plugins();
      bool build_vm();
      bool build_scripts();
      bool collect_rendering();
      void collect_scene_assets();
      void collect_model_files(const filepath& model);
      bool copy_runtime();
      bool copy_engine();
      bool copy_project();
      bool write_scenes();
      bool copy_plugins();
      bool write_project_file();
      bool write_runtime_config();
      bool swap_in();
    };

    bool compile_run::read_project() {
      const opt<filepath> found = project::find_project_file(settings.project_file);
      if (!found.has_value()) {
        return fail(std::format("'{}' is not a project file, nor a folder holding a .toml or .oproj file of its own name", settings.project_file.string()));
      }
      std::error_code ec;
      project_file = std::filesystem::absolute(*found, ec);
      project_folder = project_file.parent_path();
      project_name = project_folder.filename().string();
      base = project_folder.parent_path();
      debug = settings.config == package_config::DEBUG;

      const opt<std::string> text = read_text(project_file);
      if (!text.has_value()) {
        return fail(std::format("project file '{}' cannot be read", project_file.string()));
      }
      project_text = *text;
      try {
        table = toml::parse(project_text);
      } catch (const std::exception& e) {
        return fail(std::format("'{}' is not a project file: {}", project_file.string(), e.what()));
      }

      if (settings.engine.has_value()) {
        engine = *settings.engine;
      } else if (const opt<engine_sources> located = locate_engine_sources(settings.config, report.errors); located.has_value()) {
        engine = *located;
      } else {
        return false;
      }
      /// a listing needs the engine's root and none of the things a package is made from
      if (listing_only) {
        return true;
      }
      if (!std::filesystem::is_regular_file(engine.runtime_executable, ec)) {
        return fail(std::format("the runtime to package, '{}', does not exist", engine.runtime_executable.string()));
      }

      if (debug) {
        warn("a Debug package needs the debug C runtime, so it starts only on a machine that has Visual Studio; ship the Release package");
      }
      return true;
    }

    bool compile_run::read_package_rules() {
      const auto read = [this](const std::string_view key, glob_set& into) -> bool {
        const toml::node_view listed = table.at_path(std::format("package.{}", key));
        if (!listed) {
          return true;
        }
        const toml::array* patterns = listed.as_array();
        if (patterns == nullptr) {
          return fail(std::format("[package] {} is a list of patterns, such as [ \"assets/audio/**\" ]", key));
        }
        for (const toml::node& entry : *patterns) {
          const opt<std::string> pattern = entry.value<std::string>();
          if (!pattern.has_value()) {
            return fail(std::format("[package] {} holds something that is not a pattern in quotes", key));
          }
          if (const opt<std::string> reason = invalid_pattern_reason(*pattern); reason.has_value()) {
            return fail(std::format("[package] {} has the pattern '{}', which cannot be used: {}", key, *pattern, *reason));
          }
          into.include(*pattern);
        }
        return true;
      };
      if (const toml::node_view console = table.at_path("package.console"); console && !console.is_boolean()) {
        return fail("[package] console is true or false: whether the packaged game has the engine's console");
      }
      return read("keep", kept) && read("exclude", excluded);
    }

    bool compile_run::check_names_and_output() {
      /// the caller's, or the one the project gives by itself
      public_name = settings.public_name.value_or(default_public_name(table, project_name));
      if (const opt<std::string> reason = invalid_public_name_reason(public_name); reason.has_value()) {
        return fail(std::format("the public name '{}' cannot be used: {}", public_name, *reason));
      }

      std::error_code ec;
      output = std::filesystem::absolute(settings.output_directory, ec).lexically_normal();
      if (output == project_folder || inside(project_folder, output).has_value()) {
        return fail(std::format("the output folder '{}' is inside the project, which would put the package in its own copy; choose one outside it", output.string()));
      }

      /// a Debug package never lands on a Release one
      const std::string package_name = debug ? std::format("{}-debug", public_name) : public_name;
      final_folder = output / package_name;
      staging = output / kStagingFolder / package_name;
      staged_project = staging / project_name;
      if (std::filesystem::exists(final_folder, ec) && !std::filesystem::exists(final_folder / kPackageMarker, ec)) {
        return fail(std::format("'{}' exists and was not made by a compile, so it will not be replaced; remove it or choose another output folder", final_folder.string()));
      }
      return true;
    }

    bool compile_run::check_scenes() {
      std::string names;
      if (const toml::array* listed = table.at_path("scene-graph.scenes").as_array(); listed != nullptr) {
        for (const toml::node& entry : *listed) {
          const toml::table* row = entry.as_table();
          if (row == nullptr) {
            continue;
          }
          const std::string name = row->at_path("name").value_or(std::string{});
          const std::string written = row->at_path("path").value_or(std::string{});
          const filepath file = project_path(written);
          std::error_code ec;
          if (file.empty() || !std::filesystem::is_regular_file(file, ec)) {
            return fail(std::format("scene '{}' is listed as '{}', which does not exist", name, written));
          }
          const opt<filepath> relative = inside(project_folder, file);
          if (!relative.has_value()) {
            return fail(std::format("scene '{}' is at '{}', outside the project's folder, so it cannot be packaged with it", name, file.string()));
          }
          /// read now, so a scene that cannot be read stops the compile before anything is
          ///  written; the same document is what the package gets, in binary
          serialization::scene_parse_result parsed = serialization::load_scene_document(file);
          if (!parsed.success()) {
            return fail(std::format("scene '{}' ('{}') cannot be read: {}", name, written, parsed.error));
          }
          const size_t repaired = serialization::rewrite_scene_asset_refs(*parsed.document, [&](const std::string& id) -> std::string {
            return packaged_asset_id(id, name);
          });
          if (repaired > 0) {
            CORE_LOG_INFO("[COMPILE] : scene '{}': {} asset id(s) written through the working directory now name the project's mount or the engine's", name, repaired);
          }
          filepath packaged = *relative;
          packaged.replace_extension(serialization::kSceneBinaryExtension);
          const std::string source_key = file_key(*relative);
          for (const scene_entry& other : scenes) {
            if (other.packaged == packaged && other.source_key != source_key) {
              return fail(std::format("scenes '{}' and '{}' would both be packaged as '{}'; one of their files has to be renamed", other.name, name, packaged.generic_string()));
            }
          }
          scenes.push_back({
            .name = name,
            .written = written,
            .file = file,
            .source_key = source_key,
            .packaged = std::move(packaged),
            .document = std::move(*parsed.document),
          });
          names += std::format("{}{}", names.empty() ? "" : ", ", name);
        }
      }
      if (scenes.empty()) {
        return fail("the project has no scenes, so there is nothing for a package to start in");
      }
      if (listing_only) {
        return true;
      }

      starting_scene = table.at_path("scene-graph.starting-scene").value_or(std::string{});
      starting_scene = settings.starting_scene.value_or(starting_scene);
      if (starting_scene.empty()) {
        return fail(std::format("the project has no starting scene; choose one of: {}", names));
      }
      if (std::ranges::find(scenes, starting_scene, &scene_entry::name) == scenes.end()) {
        return fail(std::format("the starting scene '{}' is not one of the project's scenes: {}", starting_scene, names));
      }
      return true;
    }

    bool compile_run::check_scripts() {
      std::error_code ec;
      if (const opt<std::string> csproj = table.at_path("scripting.cs_project").value<std::string>(); csproj.has_value()) {
        if (const filepath file = project_path(*csproj); file.empty() || !std::filesystem::is_regular_file(file, ec)) {
          return fail(std::format("the project's C# project, '{}', does not exist", *csproj));
        }
      } else if (const opt<std::string> assembly = table.at_path("scripting.assembly").value<std::string>(); assembly.has_value()) {
        if (const filepath file = project_path(*assembly); file.empty() || !std::filesystem::is_regular_file(file, ec)) {
          return fail(std::format("the project's script assembly, '{}', does not exist", *assembly));
        }
      }
      return true;
    }

    bool compile_run::check_plugins() {
      const toml::array* listed = table.at_path("project.plugins").as_array();
      if (listed == nullptr) {
        return true;
      }
      const std::string_view built = debug ? "Debug" : "Release";
      for (const toml::node& entry : *listed) {
        const toml::table* row = entry.as_table();
        const opt<std::string> name = row == nullptr ? std::nullopt : row->at_path("name").value<std::string>();
        const opt<std::string> written = row == nullptr ? std::nullopt : row->at_path("path").value<std::string>();
        if (!name.has_value() || !written.has_value()) {
          return fail("a plugin in the project file has no name or no path; each needs both to be packaged");
        }

        /// ${build-config} stands for the configuration of whichever program reads the path. the
        ///  one that matters here is the packaged runtime's, which a plugin has to match, and
        ///  not this tool's
        std::string for_package = *written;
        constexpr std::string_view kBuildConfigTag = "${build-config}";
        for (size_t at = for_package.find(kBuildConfigTag); at != std::string::npos; at = for_package.find(kBuildConfigTag, at + built.size())) {
          for_package.replace(at, kBuildConfigTag.size(), built);
        }

        const filepath file = project_path(for_package);
        std::error_code ec;
        if (file.empty() || !std::filesystem::is_regular_file(file, ec)) {
          return fail(std::format("plugin '{}' is listed as '{}', and there is no {} build of it at '{}'; build the plugin in that configuration first", *name, *written, built,
                                  for_package));
        }
        if (file.extension() != ".dll") {
          return fail(std::format("plugin '{}' is '{}', which is not a .dll", *name, file.string()));
        }
        /// they share one folder in the package
        if (std::ranges::any_of(plugins, [&file](const plugin_entry& other) -> bool { return other.file.filename() == file.filename(); })) {
          return fail(std::format("two plugins are both named '{}'; a package keeps its plugins in one folder, so each needs its own file name", file.filename().string()));
        }
        plugins.push_back({ .name = *name, .written = *written, .file = file });
      }
      return true;
    }

    bool compile_run::build_vm() {
      PROFILE_SECTION("compile_project--vm");
      diagnostic_engine diagnostics;
      /// the packaged config starts the game in the package's folder, so this is where the
      ///  boot program finds the project file
      const std::string load_path = std::format("{}/{}", project_name, project_file.filename().string());
      project_program_result program = build_project_program(bus, project_file, &diagnostics, { .project_load_path = load_path });
      for (const std::string& warning : program.warnings) {
        warn(warning);
      }
      if (!program.success) {
        return fail("the project's VM sources did not build; the errors are in the log");
      }
      vm_sources = std::move(program.sources);
      if (!ocmd_toolchain{}.write_obin(staged_project / kBootProgram, program.bytes)) {
        return fail(std::format("could not write '{}'", (staged_project / kBootProgram).string()));
      }
      return true;
    }

    bool compile_run::build_scripts() {
      PROFILE_SECTION("compile_project--scripts");
      const filepath destination = staged_project / "bin";
      if (const opt<std::string> csproj = table.at_path("scripting.cs_project").value<std::string>(); csproj.has_value()) {
        const script_assembly_result built = build_script_assembly({
          .csproj = project_path(*csproj),
          .configuration = debug ? "Debug" : "Release",
          .destination = destination,
          .copy_symbols = debug,
          .should_stop = [this]() -> bool { return cancelled(); },
        });
        if (cancelled()) {
          return false;
        }
        if (!built.success) {
          report.errors.append_range(built.errors);
          return fail("the project's C# project did not build");
        }
        packaged_assembly = std::format("{}/bin/{}", project_name, built.assembly.filename().string());
      } else if (const opt<std::string> assembly = table.at_path("scripting.assembly").value<std::string>(); assembly.has_value()) {
        /// already built by someone else: it ships as it is
        const filepath file = project_path(*assembly);
        if (!copy(file, destination / file.filename())) {
          return false;
        }
        packaged_assembly = std::format("{}/bin/{}", project_name, file.filename().string());
      }
      return true;
    }

    bool compile_run::collect_rendering() {
      PROFILE_SECTION("compile_project--pipelines");
      const opt<project_rendering_settings> rendering = read_project_rendering_settings(project_file);
      if (!rendering.has_value()) {
        return fail("the project's [rendering] table could not be read");
      }
      const size_t errors_before = report.errors.size();
      const ostd::vector<filepath> files = project_pipeline_files(*rendering, report.errors);
      if (report.errors.size() != errors_before) {
        return false;
      }
      pipeline_collection collection = collect_pipelines(files);
      if (!collection.success) {
        report.errors.append_range(collection.errors);
        return false;
      }

      std::string names;
      for (const collected_pipeline& pipeline : collection.pipelines) {
        names += std::format("{}{}", names.empty() ? "" : ", ", pipeline.name);
      }
      /// the caller's choice, then the project's, then the only one there is
      main_pipeline = settings.main_pipeline.value_or(rendering->main_pipeline.value_or(collection.pipelines.size() == 1 ? collection.pipelines.front().name : std::string{}));
      /// which pipeline reaches the window is the caller's to choose, and a listing has no
      ///  caller's choice yet: the files are the same whichever it is
      if (!listing_only && main_pipeline.empty()) {
        return fail(std::format("the project draws with several pipelines and does not say which one reaches the window; choose one of: {}", names));
      }
      if (!listing_only && std::ranges::find(collection.pipelines, main_pipeline, &collected_pipeline::name) == collection.pipelines.end()) {
        return fail(std::format("the main pipeline '{}' is not one of the project's pipelines: {}", main_pipeline, names));
      }

      /// already in load order
      for (const collected_pipeline& pipeline : collection.pipelines) {
        ostd::vector<filepath> needed = pipeline.files;
        needed.push_back(pipeline.source);
        for (const filepath& file : needed) {
          const std::string entry = packaged_entry(file);
          if (entry.empty()) {
            return fail(std::format("pipeline '{}' needs '{}', which is in neither the project nor the engine's resources, so it cannot be packaged", pipeline.name, file.string()));
          }
          need(file, std::format("pipeline '{}'", pipeline.name));
        }
        config_pipelines.push_back(packaged_entry(pipeline.source));
      }
      return true;
    }

    void compile_run::collect_scene_assets() {
      PROFILE_SECTION("compile_project--scene-assets");
      /// everything the scenes reach: the engine's files (a stock model, say) have to come
      ///  along, and the project's are the ones a package keeps. the scenes are asked as the
      ///  package will have them, so an id that was repaired leads where the game will look
      const std::string through_working_directory = std::format("{}{}", file_system::kCwd, file_system::kPathSeparator);
      ostd::vector<filepath> roots;
      std::error_code ec;
      for (const scene_entry& scene : scenes) {
        const std::string user = std::format("scene '{}'", scene.name);
        if (const filepath script = scene.document.script.empty() ? filepath{} : resolve_relative(scene.file, scene.document.script); std::filesystem::is_regular_file(script, ec)) {
          need(script, user);
        }
        for (const serialization::component_asset_ref& ref : serialization::collect_scene_asset_refs(scene.document)) {
          /// one that could not be repaired has been warned about already
          if (ref.path.starts_with(through_working_directory)) {
            continue;
          }
          const asset_path resolved = resolve_asset_path(ref.path, scene.file);
          if (!resolved.found()) {
            warn(std::format("{} names '{}', which cannot be found; it is not in the package", user, ref.path));
            continue;
          }
          /// the dependency graph only takes a file that is under a mount
          const filepath file = resolved.resolved.lexically_normal();
          if (inside(project_folder, file).has_value() || inside(engine.root, file).has_value()) {
            roots.push_back(file);
          } else {
            need(file, user);
          }
        }
      }
      std::ranges::sort(roots);
      const auto repeated = std::ranges::unique(roots);
      roots.erase(repeated.begin(), repeated.end());

      asset_resolver resolver;
      const dependency_snapshot snapshot = resolver.resolve(roots);
      for (const dependency_snapshot::node& node : snapshot.nodes) {
        const filepath file = absolute_of(node.virtual_path).lexically_normal();
        if (!std::filesystem::is_regular_file(file, ec)) {
          continue;
        }
        need(file, "a scene");
        if (node.type == asset::MODEL_SOURCE) {
          collect_model_files(file);
        }
      }
    }

    void compile_run::collect_model_files(const filepath& model) {
      PROFILE_SECTION("compile_project--model-files");
      /// the dependency graph has a model's textures only for some formats. the importer the
      ///  game loads the model with names them for every format, so it is asked
      const std::string user = std::format("model '{}'", model.filename().string());
      const model_import_result imported = import(model);
      if (!imported.data.has_value()) {
        warn(std::format("model '{}' could not be read, so the textures it names are not known: {}", model.string(), imported.error));
        return;
      }
      std::error_code ec;
      for (const imported_material& material : imported.data->materials) {
        for (const std::string* authored : { &material.base_color_texture, &material.normal_texture, &material.metallic_roughness_texture, &material.emissive_texture }) {
          /// one that is not there is the model's own trouble: it draws with a fallback
          if (authored->empty() || authored->starts_with("embedded:")) {
            continue;
          }
          if (const filepath texture = resolve_relative(model, *authored); std::filesystem::is_regular_file(texture, ec)) {
            need(texture, user);
          }
        }
      }

      /// a .gltf keeps its geometry in files beside it
      if (model.extension() != ".gltf") {
        return;
      }
      const nlohmann::json document = nlohmann::json::parse(read_text(model).value_or(std::string{}), nullptr, false);
      if (!document.is_object() || !document.contains("buffers") || !document["buffers"].is_array()) {
        return;
      }
      for (const nlohmann::json& buffer : document["buffers"]) {
        if (!buffer.is_object() || !buffer.contains("uri") || !buffer["uri"].is_string()) {
          continue;
        }
        const std::string uri = buffer["uri"].get<std::string>();
        if (uri.empty() || uri.starts_with("data:")) {
          continue;
        }
        if (const filepath data = resolve_relative(model, uri); std::filesystem::is_regular_file(data, ec)) {
          need(data, user);
        }
      }
    }

    bool compile_run::copy_runtime() {
      if (!copy(engine.runtime_executable, staging / std::format("{}.exe", public_name))) {
        return false;
      }
      std::error_code ec;
      for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(engine.runtime_executable.parent_path(), ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".dll" && !copy(entry.path(), staging / entry.path().filename())) {
          return false;
        }
      }
      /// the debugger looks for the symbols under the name the linker gave them
      filepath symbols = engine.runtime_executable;
      symbols.replace_extension(".pdb");
      if (debug && std::filesystem::is_regular_file(symbols, ec)) {
        return copy(symbols, staging / symbols.filename());
      }
      return true;
    }

    bool compile_run::copy_engine() {
      std::error_code ec;
      const filepath resources = engine.root / "resources";
      /// the UI font is opened at start-up whether or not the UI is on
      for (std::filesystem::directory_iterator entry(resources / "fonts", ec), end; !ec && entry != end; entry.increment(ec)) {
        if (entry->is_regular_file(ec)) {
          engine_files.push_back(entry->path());
        }
      }
      if (const filepath layout = resources / "ui" / "default_ui_layout.ini"; std::filesystem::is_regular_file(layout, ec)) {
        engine_files.push_back(layout);
      }

      std::ranges::sort(engine_files);
      const auto repeated = std::ranges::unique(engine_files);
      engine_files.erase(repeated.begin(), repeated.end());
      for (const filepath& file : engine_files) {
        const opt<filepath> relative = inside(engine.root, file);
        OTHER_ASSERT(relative.has_value(), "'{}' was collected as an engine file and is not under the engine root", file.string());
        if (!copy(file, staging / "engine" / *relative)) {
          return false;
        }
      }

      for (std::filesystem::recursive_directory_iterator entry(engine.lua, ec), end; !ec && entry != end; entry.increment(ec)) {
        if (entry->is_regular_file(ec) && entry->path().extension() == ".lua" && !copy(entry->path(), staging / "engine" / "lua" / *inside(engine.lua, entry->path()))) {
          return false;
        }
      }

      const filepath assemblies = staging / "engine" / "dotnet-assemblies";
      for (const std::string_view name : { "OtherCs", "OtherCsBindings" }) {
        filepath assembly = engine.assemblies / name;
        assembly += ".dll";
        if (!std::filesystem::is_regular_file(assembly, ec)) {
          return fail(std::format("the engine assembly '{}' does not exist", assembly.string()));
        }
        if (!copy(assembly, assemblies / assembly.filename())) {
          return false;
        }
        filepath symbols = assembly;
        symbols.replace_extension(".pdb");
        if (debug && std::filesystem::is_regular_file(symbols, ec) && !copy(symbols, assemblies / symbols.filename())) {
          return false;
        }
      }
      if (!std::filesystem::is_regular_file(engine.dotnet_runtime_config, ec)) {
        return fail(std::format("the .NET runtime config '{}' does not exist", engine.dotnet_runtime_config.string()));
      }
      return copy(engine.dotnet_runtime_config, assemblies / "OtherCsBindings.runtimeconfig.json");
    }

    bool compile_run::copy_project() {
      std::ranges::sort(used);
      std::error_code ec;
      for (std::filesystem::recursive_directory_iterator entry(project_folder, ec), end; !ec && entry != end; entry.increment(ec)) {
        const filepath relative = *inside(project_folder, entry->path());
        const std::string name = relative.generic_string();
        if (entry->is_directory(ec)) {
          /// build output, logs and source control at the top; VM sources wherever the folder is
          const bool skipped = (relative.has_parent_path() ? false : std::ranges::find(kSkippedProjectFolders, name) != kSkippedProjectFolders.end()) || name == "assets/vm";
          if (skipped) {
            entry.disable_recursion_pending();
          }
          continue;
        }
        const std::string extension = entry->path().extension().string();
        /// sources that were built into the package, and the project file, which is rewritten
        if (extension == ".cs" || extension == ".csproj" || entry->path() == project_file || std::ranges::find(vm_sources, name) != vm_sources.end()) {
          continue;
        }

        const std::string key = file_key(relative);
        const bool is_used = std::ranges::binary_search(used, key);
        const bool is_kept = kept.matches(name);
        /// a listed scene is written in binary by write_scenes. its text comes along only when
        ///  the project asks for it, for a script that loads the scene by that path
        if (std::ranges::find(scenes, key, &scene_entry::source_key) != scenes.end()) {
          if (is_kept && !copy(entry->path(), staged_project / relative)) {
            return false;
          }
          continue;
        }
        if (excluded.matches(name)) {
          if (is_used) {
            warn(std::format("'{}' is left out by [package] exclude, and something in the package uses it", name));
          }
          continue;
        }
        if (is_scene_content(entry->path()) && !is_used && !is_kept) {
          if (!listing_only) {
            CORE_LOG_DEBUG("[COMPILE] : left out '{}', which nothing uses", name);
          }
          report.left_out.push_back(name);
          report.left_out_bytes += entry->file_size(ec);
          continue;
        }
        if (!copy(entry->path(), staged_project / relative)) {
          return false;
        }
      }
      if (ec) {
        return fail(std::format("could not read the project folder '{}': {}", project_folder.string(), ec.message()));
      }
      if (!report.left_out.empty() && !listing_only) {
        CORE_LOG_INFO("[COMPILE] : left out {} file(s) that nothing in the project uses, {} bytes in all", report.left_out.size(), report.left_out_bytes);
      }
      return true;
    }

    bool compile_run::write_scenes() {
      PROFILE_SECTION("compile_project--scenes");
      for (const scene_entry& scene : scenes) {
        const filepath packaged = staged_project / scene.packaged;
        std::error_code ec;
        std::filesystem::create_directories(packaged.parent_path(), ec);
        if (!serialization::save_scene_document(scene.document, packaged)) {
          return fail(std::format("could not write scene '{}' as '{}'", scene.name, packaged.string()));
        }
      }
      return true;
    }

    bool compile_run::copy_plugins() {
      /// beside the script assembly: bin/ is the one folder of the project that a compile fills
      ///  and never copies, so nothing of the project's own can be in the way
      const filepath destination = staged_project / "bin";
      for (const plugin_entry& plugin : plugins) {
        std::error_code ec;
        if (std::filesystem::exists(destination / plugin.file.filename(), ec)) {
          return fail(std::format("plugin '{}' has the file name of the project's script assembly, '{}'; one of them has to change", plugin.name, plugin.file.filename().string()));
        }
        if (!copy(plugin.file, destination / plugin.file.filename())) {
          return false;
        }
        filepath symbols = plugin.file;
        symbols.replace_extension(".pdb");
        if (debug && std::filesystem::is_regular_file(symbols, ec) && !copy(symbols, destination / symbols.filename())) {
          return false;
        }
      }
      return true;
    }

    bool compile_run::write_project_file() {
      /// text edits, so the file keeps its comments and its order
      std::string text = project_text;
      /// a plugin's path is rewritten where it stands. the list is usually spread over several
      ///  lines, one table to a line, so the path as it was written is what is looked for
      for (const plugin_entry& plugin : plugins) {
        const std::string packaged = toml_string(std::format("{}/bin/{}", project_name, plugin.file.filename().string()));
        bool replaced = false;
        for (const std::string& literal : { toml_string(plugin.written), std::format("'{}'", plugin.written) }) {
          for (size_t at = text.find(literal); at != std::string::npos; at = text.find(literal, at + packaged.size())) {
            text.replace(at, literal.size(), packaged);
            replaced = true;
          }
        }
        if (!replaced) {
          return fail(std::format("could not find the path of plugin '{}' in the project file's text to point it at the packaged copy", plugin.name));
        }
      }
      /// a scene's path is rewritten the same way, to the binary copy
      for (const scene_entry& scene : scenes) {
        const std::string packaged = packaged_scene_path(scene.written);
        if (packaged == scene.written) {
          continue;
        }
        bool replaced = false;
        for (const auto& [literal, replacement] : { std::pair{ toml_string(scene.written), toml_string(packaged) }, std::pair{ std::format("'{}'", scene.written), std::format("'{}'", packaged) } }) {
          for (size_t at = text.find(literal); at != std::string::npos; at = text.find(literal, at + replacement.size())) {
            text.replace(at, literal.size(), replacement);
            replaced = true;
          }
        }
        if (!replaced) {
          return fail(std::format("could not find the path of scene '{}' in the project file's text to point it at the packaged copy", scene.name));
        }
      }
      text = set_toml_string(text, "vm", "boot-file", kBootProgram);
      text = remove_toml_key(text, "scripting", "cs_project");
      if (!packaged_assembly.empty()) {
        text = set_toml_string(text, "scripting", "assembly", packaged_assembly);
      }
      text = set_toml_string(text, "scene-graph", "starting-scene", starting_scene);
      const filepath packaged = staged_project / project_file.filename();
      return write_text(packaged, text) ? true : fail(std::format("could not write '{}'", packaged.string()));
    }

    bool compile_run::write_runtime_config() {
      std::string pipelines;
      for (const std::string& entry : config_pipelines) {
        pipelines += std::format("{}{}", pipelines.empty() ? "" : ", ", toml_string(entry));
      }

      std::string text = "[driver]\n";
      text += std::format("boot-file = {}\n", toml_string(std::format("{}/{}", project_name, kBootProgram)));
      text += "plugins = []\n";
      if (debug) {
        text += "vm-debug-mode = false\n";
      }
      text += "\n[application]\n";
      text += std::format("name = {}\n", toml_string(public_name));
      text += std::format("core-log-level = {}\n", toml_string(debug ? "debug" : "info"));
      text += std::format("core-log-file = {}\n", toml_string(std::format("logs/{}.log", public_name)));
      text += "auto-play-loaded-scenes = true\n";
      text += "\n[filesystem]\nwatch-assets = false\n";
      text += "\n[rendering]\n";
      text += std::format("main-pipeline = {}\n", toml_string(main_pipeline));
      text += std::format("pipelines = [ {} ]\n", pipelines);
      /// a Debug package has the engine's console, shut until the key under Esc opens it. a
      ///  Release package draws no UI at all. [package] console says otherwise for either
      const bool console = table.at_path("package.console").value_or(debug);
      text += std::format("\n[ui]\nenable = {}\n", console);
      if (console) {
        text += "console-key = true\n";
      }
      text += "\n[scripting]\n";
      text += "dotnet-runtime-config = \"engine/dotnet-assemblies/OtherCsBindings.runtimeconfig.json\"\n";
      text += "dotnet-bindings = \"engine/dotnet-assemblies/OtherCsBindings.dll\"\n";
      text += "other-csharp = \"engine/dotnet-assemblies/OtherCs.dll\"\n";
      text += "other-lua-directory = \"engine/lua\"\n";

      /// the runtime looks for a config with the executable's name beside it
      const filepath config = staging / std::format("{}.toml", public_name);
      if (!write_text(config, text)) {
        return fail(std::format("could not write '{}'", config.string()));
      }
      return write_text(staging / kPackageMarker, "this folder was made by an Other Environment project compile, which replaces it when it runs again\n") ?
        true :
        fail(std::format("could not write '{}'", (staging / kPackageMarker).string()));
    }

    bool compile_run::swap_in() {
      std::error_code ec;
      std::filesystem::remove_all(final_folder, ec);
      if (ec) {
        return fail(std::format("could not replace '{}': {}. is the game running?", final_folder.string(), ec.message()));
      }
      std::filesystem::rename(staging, final_folder, ec);
      if (ec) {
        return fail(std::format("could not move the finished package to '{}': {}", final_folder.string(), ec.message()));
      }
      /// only when nothing else is staged there
      std::filesystem::remove(output / kStagingFolder, ec);
      report.package_root = final_folder;
      return true;
    }

  }  // namespace

  std::string_view compile_stage_label(const compile_stage stage) {
    switch (stage) {
      case compile_stage::CHECKS: return "checking the project";
      case compile_stage::VM: return "building the boot program";
      case compile_stage::SCRIPTS: return "building the scripts";
      case compile_stage::PIPELINES: return "collecting the pipelines";
      case compile_stage::SCENE_ASSETS: return "finding what the scenes use";
      case compile_stage::RUNTIME: return "copying the runtime";
      case compile_stage::ENGINE: return "copying the engine's files";
      case compile_stage::PROJECT: return "copying the project";
      case compile_stage::SCENES: return "writing the scenes";
      case compile_stage::PLUGINS: return "copying the plugins";
      case compile_stage::FINISH: return "finishing the package";
      default: return "working";
    }
  }

  std::string readable_size(const uint64_t bytes) {
    constexpr uint64_t kKilobyte = 1024;
    constexpr uint64_t kMegabyte = kKilobyte * kKilobyte;
    if (bytes < kKilobyte) {
      return std::format("{} bytes", bytes);
    }
    if (bytes < kMegabyte) {
      return std::format("{:.1f} KB", static_cast<double>(bytes) / static_cast<double>(kKilobyte));
    }
    return std::format("{:.1f} MB", static_cast<double>(bytes) / static_cast<double>(kMegabyte));
  }

  opt<engine_sources> locate_engine_sources(const package_config config, ostd::vector<std::string>& errors) {
    PROFILE_SECTION("locate_engine_sources");
    const bool debug = config == package_config::DEBUG;
    const filepath root = environment_root();
    engine_sources sources = { .root = root };
    if (is_installed_build()) {
      sources.runtime_executable = root / "bin" / (debug ? "debug" : "release") / "other_runtime.exe";
      sources.assemblies = root / "dotnet-assemblies";
      sources.dotnet_runtime_config = sources.assemblies / "OtherCsBindings.runtimeconfig.json";
      sources.lua = root / "lua";
    } else {
      const std::string_view built = debug ? "Debug" : "Release";
      sources.runtime_executable = root / "build" / "other-runtime" / built / "other_runtime.exe";
      sources.assemblies = root / "build" / "other-csharp" / built;
      sources.dotnet_runtime_config = root / "other-csharp-interop" / "resources" / "OtherCsBindings.runtimeconfig.json";
      sources.lua = root / "other-lua-interop";
    }

    std::error_code ec;
    if (!std::filesystem::is_regular_file(sources.runtime_executable, ec)) {
      errors.push_back(std::format("there is no {} runtime at '{}'; that configuration of the engine has to be built before a package can be made from it",
                                   debug ? "Debug" : "Release", sources.runtime_executable.string()));
      return std::nullopt;
    }
    return sources;
  }

  opt<std::string> invalid_public_name_reason(const std::string_view name) {
    if (name.empty()) {
      return "it is empty";
    }
    for (const char c : name) {
      if (static_cast<unsigned char>(c) < 0x20 || std::string_view{ "<>:\"/\\|?*" }.contains(c)) {
        return std::format("a file name cannot hold '{}'", c);
      }
    }
    if (name.back() == '.' || name.back() == ' ' || name.front() == ' ') {
      return "a file name cannot start with a space or end with a space or a dot";
    }

    /// reserved whatever follows the first dot, and in any case
    std::string stem{ name.substr(0, name.find('.')) };
    std::ranges::transform(stem, stem.begin(), [](const unsigned char c) -> char { return static_cast<char>(std::toupper(c)); });
    if (std::ranges::find(kReservedFileNames, stem) != kReservedFileNames.end()) {
      return std::format("'{}' is a name Windows keeps for a device", stem);
    }
    return std::nullopt;
  }

  std::string set_toml_string(const std::string_view text, const std::string_view table, const std::string_view key, const std::string_view value) {
    ostd::vector<std::string> lines = split_lines(text);
    const std::string_view ending = line_ending_of(text);
    const std::string entry = std::format("{} = {}", key, toml_string(value));
    const table_position position = find_in_table(lines, table, key);

    if (position.key.has_value()) {
      /// the line's own indent and ending stay
      std::string& line = lines[*position.key];
      const size_t indent = line.find_first_not_of(" \t");
      const size_t body_end = line.find_last_not_of("\r\n");
      line = std::format("{}{}{}", line.substr(0, indent), entry, line.substr(body_end + 1));
    } else if (position.header < lines.size()) {
      if (!lines[position.header].ends_with('\n')) {
        lines[position.header] += ending;
      }
      lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(position.header) + 1, std::format("{}{}", entry, ending));
    } else {
      if (!lines.empty() && !lines.back().ends_with('\n')) {
        lines.back() += ending;
      }
      if (!lines.empty()) {
        lines.emplace_back(ending);
      }
      lines.push_back(std::format("[{}]{}", table, ending));
      lines.push_back(std::format("{}{}", entry, ending));
    }
    return joined(lines);
  }

  std::string remove_toml_key(const std::string_view text, const std::string_view table, const std::string_view key) {
    ostd::vector<std::string> lines = split_lines(text);
    const table_position position = find_in_table(lines, table, key);
    if (position.key.has_value()) {
      lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(*position.key));
    }
    return joined(lines);
  }

  compile_preview preview_project_compile(const command_bus& bus, const filepath& project_file) {
    PROFILE_SECTION("preview_project_compile");
    compile_preview preview;
    const opt<filepath> found = project::find_project_file(project_file);
    const opt<std::string> text = found.has_value() ? read_text(*found) : std::nullopt;
    if (!text.has_value()) {
      preview.problems.push_back(std::format("project file '{}' cannot be read", project_file.string()));
      return preview;
    }
    toml::table table;
    try {
      table = toml::parse(*text);
    } catch (const std::exception& e) {
      preview.problems.push_back(std::format("'{}' is not a project file: {}", found->string(), e.what()));
      return preview;
    }

    std::error_code ec;
    const filepath file = std::filesystem::absolute(*found, ec);
    preview.public_name = default_public_name(table, file.parent_path().filename().string());

    if (const toml::array* listed = table.at_path("scene-graph.scenes").as_array(); listed != nullptr) {
      for (const toml::node& entry : *listed) {
        if (const toml::table* row = entry.as_table(); row != nullptr) {
          preview.scenes.push_back(row->at_path("name").value_or(std::string{}));
        }
      }
    }
    if (preview.scenes.empty()) {
      preview.problems.push_back("the project has no scenes, so there is nothing for a package to start in");
    }
    preview.starting_scene = table.at_path("scene-graph.starting-scene").value_or(std::string{});
    if (std::ranges::find(preview.scenes, preview.starting_scene) == preview.scenes.end()) {
      preview.starting_scene.clear();
    }

    if (const opt<project_rendering_settings> rendering = read_project_rendering_settings(file); rendering.has_value()) {
      const ostd::vector<filepath> files = project_pipeline_files(*rendering, preview.problems);
      const pipeline_collection collection = collect_pipelines(files);
      preview.problems.append_range(collection.errors);
      for (const collected_pipeline& pipeline : collection.pipelines) {
        preview.pipelines.push_back(pipeline.name);
      }
      /// the project's own choice when it is one of them, or the only one there is
      const std::string named = rendering->main_pipeline.value_or(preview.pipelines.size() == 1 ? preview.pipelines.front() : std::string{});
      preview.main_pipeline = std::ranges::find(preview.pipelines, named) == preview.pipelines.end() ? std::string{} : named;
    }

    diagnostic_engine diagnostics;
    project_program_result program = build_project_program(bus, file, &diagnostics);
    if (!program.success) {
      preview.problems.push_back("the project's VM sources do not build; the errors are in the log");
    }
    preview.vm_sources = std::move(program.sources);
    preview.hooks = std::move(program.hooks);
    if (!preview.problems.empty()) {
      return preview;
    }

    /// what a compile would leave out: the stages a compile decides that with, run with
    ///  nothing copied. what stops them stops a compile too, so it is a problem here
    const compile_settings listing = { .project_file = file, .engine = engine_sources{ .root = environment_root() } };
    compile_run run = { .bus = bus, .settings = listing };
    run.listing_only = true;
    if (run.read_project()) {
      const scoped_project_environment environment(run.engine.root, run.project_name, run.project_folder);
      if (run.read_package_rules() && run.check_scenes() && run.collect_rendering()) {
        run.collect_scene_assets();
        run.vm_sources = preview.vm_sources;
        run.copy_project();
      }
    }
    preview.problems.append_range(run.report.errors);
    if (preview.problems.empty()) {
      preview.left_out = std::move(run.report.left_out);
      preview.left_out_bytes = run.report.left_out_bytes;
    }
    return preview;
  }

  opt<std::string> save_compile_choices(const filepath& project_file, const compile_choices& choices) {
    PROFILE_SECTION("save_compile_choices");
    const opt<filepath> found = project::find_project_file(project_file);
    const opt<std::string> original = found.has_value() ? read_text(*found) : std::nullopt;
    if (!original.has_value()) {
      return std::format("project file '{}' cannot be read", project_file.string());
    }
    toml::table table;
    try {
      table = toml::parse(*original);
    } catch (const std::exception& e) {
      return std::format("'{}' is not a project file: {}", found->string(), e.what());
    }

    std::error_code ec;
    const std::string project_name = std::filesystem::absolute(*found, ec).parent_path().filename().string();
    std::string text = *original;
    /// a name the project already gives by itself is not written down a second time
    if (!choices.public_name.empty() && choices.public_name != default_public_name(table, project_name)) {
      text = set_toml_string(text, "project", "public-name", choices.public_name);
    }
    if (!choices.starting_scene.empty() && choices.starting_scene != table.at_path("scene-graph.starting-scene").value_or(std::string{})) {
      text = set_toml_string(text, "scene-graph", "starting-scene", choices.starting_scene);
    }
    if (!choices.main_pipeline.empty() && choices.main_pipeline != table.at_path("rendering.main-pipeline").value_or(std::string{})) {
      text = set_toml_string(text, "rendering", "main-pipeline", choices.main_pipeline);
    }

    if (text != *original && !write_text(*found, text)) {
      return std::format("could not write '{}'", found->string());
    }
    return std::nullopt;
  }

  compile_report compile_project(const command_bus& bus, const compile_settings& settings) {
    PROFILE_SECTION("compile_project");
    compile_run run = { .bus = bus, .settings = settings };

    /// nothing is written until all of this has passed
    if (!run.enter(compile_stage::CHECKS) || !run.read_project()) {
      return run.report;
    }
    const scoped_project_environment environment(run.engine.root, run.project_name, run.project_folder);
    if (!run.read_package_rules() || !run.check_names_and_output() || !run.check_scenes() || !run.check_scripts() || !run.check_plugins()) {
      return run.report;
    }

    std::error_code ec;
    std::filesystem::remove_all(run.staging, ec);
    std::filesystem::create_directories(run.staged_project, ec);
    if (ec) {
      run.fail(std::format("could not make the staging folder '{}': {}", run.staging.string(), ec.message()));
      return run.report;
    }

    bool built = run.enter(compile_stage::VM) && run.build_vm() && run.enter(compile_stage::SCRIPTS) && run.build_scripts() && run.enter(compile_stage::PIPELINES) &&
      run.collect_rendering() && run.enter(compile_stage::SCENE_ASSETS);
    if (built) {
      run.collect_scene_assets();
      /// past the start of the last stage a compile is not stopped: the package is whole by then
      built = run.enter(compile_stage::RUNTIME) && run.copy_runtime() && run.enter(compile_stage::ENGINE) && run.copy_engine() && run.enter(compile_stage::PROJECT) &&
        run.copy_project() && run.enter(compile_stage::SCENES) && run.write_scenes() && run.enter(compile_stage::PLUGINS) && run.copy_plugins() &&
        run.enter(compile_stage::FINISH) && run.write_project_file() && run.write_runtime_config() && run.swap_in();
    }
    if (!built) {
      /// a failed compile leaves nothing half-made behind, and the package that was there stays
      std::filesystem::remove_all(run.staging, ec);
      std::filesystem::remove(run.output / kStagingFolder, ec);
      return run.report;
    }

    run.report.success = true;
    CORE_LOG_INFO("[COMPILE] : packaged '{}' as '{}'", run.project_name, run.report.package_root.string());
    return run.report;
  }

}  // namespace other
