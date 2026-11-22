/**
 * \file project/pipeline_collector.cpp
 **/
#include "project/pipeline_collector.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <toml++/toml.hpp>

#include "core/logger.hpp"
#include "core/profiler.hpp"
#include "file/path_helpers.hpp"

#include "renderer/pipeline_definition.hpp"

namespace other {
  namespace {

    opt<std::string> read_text(const filepath& path) {
      std::ifstream file(path, std::ios::binary);
      if (!file.is_open()) {
        return std::nullopt;
      }
      std::ostringstream text;
      text << file.rdbuf();
      return text.str();
    }

    opt<toml::table> parse_toml(const filepath& path) {
      const opt<std::string> text = read_text(path);
      if (!text.has_value()) {
        return std::nullopt;
      }
      try {
        return toml::parse(*text);
      } catch (const std::exception& e) {
        CORE_LOG_ERROR("[PROJECT] : '{}' is not valid TOML: {}", path.string(), e.what());
        return std::nullopt;
      }
    }

    /// the name inside the quotes of an '#include "name"' line; empty when line is not one.
    ///  the rule is the shader loader's (stb_include): the '#' is the first thing on the line
    ///  bar blanks, and a blank follows 'include'. a line behind '//' is therefore not one
    std::string_view included_name(std::string_view line) {
      const auto skip_blanks = [&line] {
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
          line.remove_prefix(1);
        }
      };
      skip_blanks();
      if (!line.starts_with('#')) {
        return {};
      }
      line.remove_prefix(1);
      skip_blanks();
      if (!line.starts_with("include")) {
        return {};
      }
      line.remove_prefix(std::string_view{ "include" }.size());
      if (!line.starts_with(' ') && !line.starts_with('\t')) {
        return {};
      }
      skip_blanks();
      if (!line.starts_with('"')) {
        return {};
      }
      line.remove_prefix(1);
      const size_t close = line.find('"');
      return close == std::string_view::npos ? std::string_view{} : line.substr(0, close);
    }

    /// file and everything it includes go into files. the loader looks for every include,
    ///  however deep, in the folder of the shader it started from, so that folder is the
    ///  only place looked in here
    void add_shader_and_includes(const filepath& file, const filepath& include_folder, const std::string& owner, ostd::vector<filepath>& files, ostd::vector<std::string>& errors) {
      if (std::ranges::find(files, file) != files.end()) {
        return;
      }
      files.push_back(file);

      const opt<std::string> text = read_text(file);
      if (!text.has_value()) {
        errors.push_back(std::format("{}: '{}' cannot be read", owner, file.string()));
        return;
      }

      std::istringstream stream(*text);
      for (std::string line; std::getline(stream, line);) {
        const std::string_view name = included_name(line);
        if (name.empty()) {
          continue;
        }
        const filepath included = (include_folder / name).lexically_normal();
        std::error_code ec;
        if (!std::filesystem::is_regular_file(included, ec)) {
          errors.push_back(std::format("{}: '{}' includes '{}', which is not at '{}'", owner, file.filename().string(), name, included.string()));
          continue;
        }
        add_shader_and_includes(included, include_folder, owner, files, errors);
      }
    }

    /// the pipelines a pipeline needs loaded before it. none can say so yet; when pipelines can
    ///  use each other this reads that key, and the ordering below is already in place for it
    ostd::vector<std::string> pipeline_dependencies(const pipeline_definition&) {
      return {};
    }

    opt<collected_pipeline> collect_one(const filepath& pipeline_file, ostd::vector<std::string>& errors) {
      /// the reader asserts on the things checked here, and a build must report them
      std::error_code ec;
      if (!std::filesystem::is_regular_file(pipeline_file, ec)) {
        errors.push_back(std::format("pipeline file '{}' does not exist", pipeline_file.string()));
        return std::nullopt;
      }
      if (pipeline_file.extension() != ".toml") {
        errors.push_back(std::format("pipeline file '{}' is not a .toml file", pipeline_file.string()));
        return std::nullopt;
      }
      const opt<toml::table> table = parse_toml(pipeline_file);
      if (!table.has_value()) {
        errors.push_back(std::format("pipeline file '{}' cannot be read as TOML", pipeline_file.string()));
        return std::nullopt;
      }
      if (table->at_path("asset-type").value_or(std::string{}) != "rendering-pipeline" || !table->contains("name") || !table->contains("version")) {
        errors.push_back(std::format("'{}' is not a rendering pipeline: it needs asset-type = \"rendering-pipeline\", a name and a version", pipeline_file.string()));
        return std::nullopt;
      }

      const pipeline_definition definition = read_pipeline_definition_from_file(pipeline_file);
      if (definition.source_path.empty()) {
        errors.push_back(std::format("pipeline file '{}' is not a valid pipeline; the log says what is wrong with it", pipeline_file.string()));
        return std::nullopt;
      }

      collected_pipeline collected = { .name = definition.name, .source = definition.source_path, .depends_on = pipeline_dependencies(definition) };
      const std::string owner = std::format("pipeline '{}'", definition.name);
      const size_t errors_before = errors.size();

      /// found the way the pipeline finds them when it is built: against its own file
      const auto find = [&](const std::string_view raw, const std::string_view what) -> opt<filepath> {
        const asset_path found = resolve_asset_path(raw, definition.source_path);
        if (!found.found()) {
          errors.push_back(std::format("{}: {} '{}' was not found", owner, what, raw));
          return std::nullopt;
        }
        return found.resolved.lexically_normal();
      };

      for (const pipeline_shader_definition& shader : definition.shaders) {
        const std::string what = std::format("shader '{}' file", shader.name);
        for (const std::string& stage : { shader.vertex_path, shader.fragment_path, shader.geometry_path.value_or(""), shader.compute_path.value_or("") }) {
          if (stage.empty()) {
            continue;
          }
          if (const opt<filepath> file = find(stage, what); file.has_value()) {
            add_shader_and_includes(*file, file->parent_path(), owner, collected.files, errors);
          }
        }
      }
      for (const pipeline_texture_definition& texture : definition.textures) {
        if (!texture.seed_texture_path.has_value()) {
          continue;
        }
        if (const opt<filepath> file = find(*texture.seed_texture_path, std::format("seed for texture '{}'", texture.name)); file.has_value()) {
          collected.files.push_back(*file);
        }
      }
      if (errors.size() != errors_before) {
        return std::nullopt;
      }

      std::ranges::sort(collected.files);
      const auto repeated = std::ranges::unique(collected.files);
      collected.files.erase(repeated.begin(), repeated.end());
      return collected;
    }

  }  // namespace

  opt<project_rendering_settings> read_project_rendering_settings(const filepath& project_file) {
    PROFILE_SECTION("read_project_rendering_settings");
    const opt<toml::table> table = parse_toml(project_file);
    if (!table.has_value()) {
      CORE_LOG_ERROR("[PROJECT] : project file '{}' cannot be read", project_file.string());
      return std::nullopt;
    }

    project_rendering_settings settings = { .project_file = project_file };
    if (const toml::array* listed = table->at_path("rendering.pipelines").as_array(); listed != nullptr) {
      for (const toml::node& entry : *listed) {
        if (const opt<std::string> path = entry.value<std::string>(); path.has_value() && !path->empty()) {
          settings.pipelines.push_back(*path);
        }
      }
    }
    if (settings.pipelines.empty()) {
      settings.pipelines.push_back(std::string{ kDefaultProjectPipeline });
    }
    if (const opt<std::string> name = table->at_path("rendering.main-pipeline").value<std::string>(); name.has_value() && !name->empty()) {
      settings.main_pipeline = *name;
    }
    return settings;
  }

  ostd::vector<filepath> project_pipeline_files(const project_rendering_settings& settings, ostd::vector<std::string>& errors) {
    PROFILE_SECTION("project_pipeline_files");
    ostd::vector<filepath> files;
    for (const std::string& entry : settings.pipelines) {
      /// the project file is the file the path is written in, so its folder is tried first
      const asset_path found = resolve_asset_path(entry, settings.project_file);
      std::error_code ec;
      if (!found.found() || !std::filesystem::is_regular_file(found.resolved, ec)) {
        errors.push_back(std::format("rendering pipeline '{}' in '{}' does not exist", entry, settings.project_file.filename().string()));
        continue;
      }
      files.push_back(found.resolved.lexically_normal());
    }
    return files;
  }

  bool order_pipelines_by_dependency(ostd::vector<collected_pipeline>& pipelines, ostd::vector<std::string>& errors) {
    PROFILE_SECTION("order_pipelines_by_dependency");
    enum class mark : uint8_t {
      UNSEEN,
      OPEN,
      DONE,
    };
    ostd::vector<mark> marks(pipelines.size(), mark::UNSEEN);
    ostd::vector<size_t> order;
    ostd::vector<size_t> chain;
    bool ok = true;

    const auto index_of = [&pipelines](const std::string_view name) -> opt<size_t> {
      const auto it = std::ranges::find(pipelines, name, &collected_pipeline::name);
      return it == pipelines.end() ? std::nullopt : opt<size_t>{ static_cast<size_t>(it - pipelines.begin()) };
    };

    /// depth first: a pipeline is placed once everything it needs has been
    const auto visit = [&](this const auto& self, const size_t at) -> void {
      if (marks[at] == mark::DONE) {
        return;
      }
      chain.push_back(at);
      if (marks[at] == mark::OPEN) {
        std::string circle;
        for (auto it = std::ranges::find(chain, at); it != chain.end(); ++it) {
          circle += std::format("{}{}", circle.empty() ? "" : " -> ", pipelines[*it].name);
        }
        errors.push_back(std::format("pipelines need each other in a circle: {}", circle));
        ok = false;
        chain.pop_back();
        return;
      }

      marks[at] = mark::OPEN;
      for (const std::string& needed : pipelines[at].depends_on) {
        if (const opt<size_t> next = index_of(needed); next.has_value()) {
          self(*next);
        } else {
          errors.push_back(std::format("pipeline '{}' needs '{}', which is not one of the pipelines being collected", pipelines[at].name, needed));
          ok = false;
        }
      }
      marks[at] = mark::DONE;
      order.push_back(at);
      chain.pop_back();
    };
    for (size_t i = 0; i < pipelines.size(); ++i) {
      visit(i);
    }
    if (!ok) {
      return false;
    }

    ostd::vector<collected_pipeline> ordered;
    ordered.reserve(pipelines.size());
    for (const size_t at : order) {
      ordered.push_back(std::move(pipelines[at]));
    }
    pipelines = std::move(ordered);
    return true;
  }

  pipeline_collection collect_pipelines(std::span<const filepath> pipeline_files) {
    PROFILE_SECTION("collect_pipelines");
    pipeline_collection collection;
    ostd::vector<filepath> seen;
    for (const filepath& file : pipeline_files) {
      std::error_code ec;
      const filepath canonical = std::filesystem::weakly_canonical(file, ec);
      const filepath& key = ec ? file : canonical;
      /// the same file listed twice is one pipeline
      if (std::ranges::find(seen, key) != seen.end()) {
        continue;
      }
      seen.push_back(key);

      opt<collected_pipeline> collected = collect_one(file, collection.errors);
      if (!collected.has_value()) {
        continue;
      }
      if (const auto same = std::ranges::find(collection.pipelines, collected->name, &collected_pipeline::name); same != collection.pipelines.end()) {
        collection.errors.push_back(std::format("two pipelines are named '{}': '{}' and '{}'", collected->name, same->source.string(), collected->source.string()));
        continue;
      }
      collection.pipelines.push_back(std::move(*collected));
    }

    if (collection.errors.empty()) {
      order_pipelines_by_dependency(collection.pipelines, collection.errors);
    }
    collection.success = collection.errors.empty();
    if (!collection.success) {
      for (const std::string& error : collection.errors) {
        CORE_LOG_ERROR("[PROJECT] : {}", error);
      }
      collection.pipelines.clear();
    }
    return collection;
  }

}  // namespace other
