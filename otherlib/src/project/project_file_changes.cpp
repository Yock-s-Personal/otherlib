/**
 * \file project/project_file_changes.cpp
 **/
#include "project/project_file_changes.hpp"

#include <sstream>

#include <toml++/toml.hpp>

#include "core/profiler.hpp"

namespace other {
  namespace {

    /// a part of a project file as text, so two versions of it can be compared whatever
    ///  kind of value it is. empty when the file does not have the part
    std::string part_of(const toml::table& table, const std::string_view path) {
      const toml::node_view part = table.at_path(path);
      if (!part) {
        return {};
      }
      std::ostringstream text;
      text << toml::json_formatter{ *part.node() };
      return text.str();
    }

  }  // namespace

  project_file_changes compare_project_files(const std::string_view loaded, const std::string_view on_disk) {
    PROFILE_SECTION("compare_project_files");
    project_file_changes changes;
    toml::table before;
    toml::table after;
    try {
      after = toml::parse(on_disk);
    } catch (const std::exception& e) {
      changes.error = std::format("the project file on disk cannot be read: {}", e.what());
      return changes;
    }
    try {
      before = toml::parse(loaded);
    } catch (const std::exception&) {
      /// what was loaded did parse once; treated as empty, so everything in the new file differs
    }

    changes.metadata_changed = part_of(before, "project.metadata") != part_of(after, "project.metadata");
    if (const std::string now = after.at_path("scene-graph.starting-scene").value_or(std::string{}); now != before.at_path("scene-graph.starting-scene").value_or(std::string{})) {
      changes.starting_scene = now;
    }

    constexpr std::pair<std::string_view, std::string_view> kReadAtLoad[] = {
      { "scene-graph.scenes", "the scene list" },
      { "scene-graph.graph", "the scene graph" },
      { "scripting", "the scripting table" },
      { "project.plugins", "the plugins" },
      { "filesystem", "the filesystem table" },
    };
    for (const auto& [path, words] : kReadAtLoad) {
      if (part_of(before, path) != part_of(after, path)) {
        changes.needs_reopen.emplace_back(words);
      }
    }
    return changes;
  }

}  // namespace other
