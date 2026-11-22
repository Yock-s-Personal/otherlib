/**
 * \file scripting/dotnet_bindings/project_bindings.cpp
 **/
#include "scripting/dotnet_bindings/project_bindings.hpp"

#include <algorithm>

#include <xxHash/xxh3.h>

#include "core/profiler.hpp"

#include "driver/driver.hpp"
#include "driver/systems/project_system.hpp"
#include "project/project.hpp"
#include "scripting/dotnet_bindings/driver_bindings.hpp"
#include "scripting/main_thread_only.hpp"

namespace other {
  namespace bindings {

    namespace {

      struct project_bridge {
        /// the copy scripts are reading from, what it holds, and whether a frame has started
        ///  since it was taken
        script_project current;
        natural_t current_stamp = 0;
        bool stale = true;
        const script_project* override_project = nullptr;
        natural_t rebuild_requests = 0;
        natural_t string_reads = 0;
      };

      project_bridge& bridge() {
        static project_bridge state;
        return state;
      }

      project_system* driver_projects() {
        driver* d = detail::get_dotnet_native_driver_unchecked();
        if (d == nullptr || !d->get_kernel().has_core_system<project_system>()) {
          return nullptr;
        }

        project_system& projects = d->get_kernel().get_core_system<project_system>();
        return projects.has_project() ? &projects : nullptr;
      }

      /// refilled in place: the strings keep their memory, so a frame in which the project
      ///  is what it was allocates nothing here
      void copy_into(script_project& copy, project_system& projects) {
        PROFILE_SECTION("bindings::copy-project-for-scripts");
        const project& current = projects.get_project();

        copy.state = static_cast<int32_t>(current.get_state());
        if (current.is_empty()) {
          const int32_t state = copy.state;
          copy = script_project{};
          copy.state = state;
          return;
        }

        const project::metadata& meta = current.get_metadata();
        copy.name = meta.name;
        copy.version = meta.version;
        copy.author = meta.author;
        copy.description = meta.description;
        copy.file.clear();
        if (const opt<filepath>& file = projects.get_project_file(); file.has_value()) {
          copy.file = file->string();
        }

        /// a dropped starting scene leaves id 0: nothing is activated until the user picks one
        const auto scenes = current.get_scenes();
        copy.scene_count = static_cast<int32_t>(scenes.size());
        const natural_t start_id = current.get_starting_scene_id();
        const auto start = std::ranges::find(scenes, start_id, &project::scene::scene_id);
        copy.starting_scene.clear();
        if (start_id != 0 && start != scenes.end()) {
          copy.starting_scene = start->name;
        }

        const opt<project::file_notice>& notice = current.get_file_notice();
        copy.file_notice = notice.has_value();
        copy.file_notice_error.clear();
        copy.file_notice_needs_reopen.clear();
        if (notice.has_value()) {
          copy.file_notice_error = notice->error;
          copy.file_notice_needs_reopen.assign(notice->needs_reopen.begin(), notice->needs_reopen.end());
        }

        copy.load_errors.clear();
        for (const load_error& error : current.get_load_errors()) {
          copy.load_errors.push_back({ .subject = error.subject, .message = error.message });
        }

        copy.script_project_path = current.script_project_path().string();
        copy.scripts_building = projects.script_project_building();
        copy.scripts_loaded = current.script_project_mounted();

        const opt<script_failure>& failure = current.get_script_failure();
        copy.script_failure = failure.has_value();
        copy.script_failure_project_path.clear();
        copy.script_failure_reason.clear();
        copy.script_failure_details.clear();
        copy.script_failure_can_retry = true;
        if (failure.has_value()) {
          copy.script_failure_project_path = failure->csproj_path.string();
          copy.script_failure_reason = failure->reason;
          copy.script_failure_details = failure->details;
          copy.script_failure_can_retry = failure->can_retry;
        }
      }

      const script_project& project() {
        return bridge().override_project != nullptr ? *bridge().override_project : bridge().current;
      }

      const char* text(const std::string& value) {
        ++bridge().string_reads;
        return returned_string(value);
      }

    }  // namespace

    natural_t script_project::stamp() const {
      natural_t hash = 0;
      const auto mix_text = [&hash](const std::string& value) {
        /// the length goes in with the seed, so "ab","c" and "a","bc" are not the same
        hash = XXH3_64bits_withSeed(value.data(), value.size(), hash + value.size() + 1);
      };
      const auto mix_number = [&hash](natural_t value) {
        hash = XXH3_64bits_withSeed(&value, sizeof(value), hash);
      };

      mix_number(static_cast<natural_t>(state));
      mix_text(name);
      mix_text(version);
      mix_text(author);
      mix_text(description);
      mix_text(file);
      mix_number(static_cast<natural_t>(scene_count));
      mix_text(starting_scene);

      mix_number(file_notice ? 1u : 0u);
      mix_text(file_notice_error);
      mix_number(file_notice_needs_reopen.size());
      for (const std::string& part : file_notice_needs_reopen) {
        mix_text(part);
      }

      mix_number(load_errors.size());
      for (const load_error& error : load_errors) {
        mix_text(error.subject);
        mix_text(error.message);
      }

      mix_text(script_project_path);
      mix_number((scripts_building ? 1u : 0u) | (scripts_loaded ? 2u : 0u) | (script_failure ? 4u : 0u) | (script_failure_can_retry ? 8u : 0u));
      mix_text(script_failure_project_path);
      mix_text(script_failure_reason);
      mix_text(script_failure_details);
      return hash;
    }

    natural_t native_project_change_stamp() {
      PROFILE_SECTION("bindings::native_project_change_stamp");
      if (bridge().override_project != nullptr) {
        return bridge().override_project->stamp();
      }

      if (bridge().stale) {
        if (project_system* projects = driver_projects(); projects != nullptr) {
          copy_into(bridge().current, *projects);
        } else {
          bridge().current = script_project{};
        }
        bridge().current_stamp = bridge().current.stamp();
        bridge().stale = false;
      }
      return bridge().current_stamp;
    }

    void begin_script_project_frame() {
      bridge().stale = true;
    }

    int32_t native_project_get_state() {
      return project().state;
    }

    const char* native_project_get_name() {
      return text(project().name);
    }

    const char* native_project_get_version() {
      return text(project().version);
    }

    const char* native_project_get_author() {
      return text(project().author);
    }

    const char* native_project_get_description() {
      return text(project().description);
    }

    const char* native_project_get_file() {
      return text(project().file);
    }

    int32_t native_project_get_scene_count() {
      return project().scene_count;
    }

    const char* native_project_get_starting_scene() {
      return text(project().starting_scene);
    }

    nbool32 native_project_has_file_notice() {
      return project().file_notice;
    }

    const char* native_project_get_file_notice_error() {
      return text(project().file_notice_error);
    }

    int32_t native_project_get_file_notice_part_count() {
      return static_cast<int32_t>(project().file_notice_needs_reopen.size());
    }

    const char* native_project_get_file_notice_part(int32_t index) {
      const auto& parts = project().file_notice_needs_reopen;
      if (index < 0 || static_cast<size_t>(index) >= parts.size()) {
        return "";
      }
      return text(parts[static_cast<size_t>(index)]);
    }

    int32_t native_project_get_load_error_count() {
      return static_cast<int32_t>(project().load_errors.size());
    }

    const char* native_project_get_load_error_subject(int32_t index) {
      const auto& errors = project().load_errors;
      if (index < 0 || static_cast<size_t>(index) >= errors.size()) {
        return "";
      }
      return text(errors[static_cast<size_t>(index)].subject);
    }

    const char* native_project_get_load_error_message(int32_t index) {
      const auto& errors = project().load_errors;
      if (index < 0 || static_cast<size_t>(index) >= errors.size()) {
        return "";
      }
      return text(errors[static_cast<size_t>(index)].message);
    }

    const char* native_project_get_script_project_path() {
      return text(project().script_project_path);
    }

    nbool32 native_project_scripts_building() {
      return project().scripts_building;
    }

    nbool32 native_project_scripts_loaded() {
      return project().scripts_loaded;
    }

    nbool32 native_project_has_script_failure() {
      return project().script_failure;
    }

    const char* native_project_get_script_failure_project_path() {
      return text(project().script_failure_project_path);
    }

    const char* native_project_get_script_failure_reason() {
      return text(project().script_failure_reason);
    }

    const char* native_project_get_script_failure_details() {
      return text(project().script_failure_details);
    }

    nbool32 native_project_script_failure_can_retry() {
      return project().script_failure_can_retry;
    }

    void native_project_request_script_rebuild() {
      ++bridge().rebuild_requests;
      if (bridge().override_project != nullptr) {
        return;
      }
      if (project_system* projects = driver_projects(); projects != nullptr) {
        projects->request_script_rebuild();
      }
    }

    void set_script_project(const script_project* project) {
      bridge().override_project = project;
    }

    natural_t script_project_rebuild_requests() {
      return bridge().rebuild_requests;
    }

    natural_t script_project_string_reads() {
      return bridge().string_reads;
    }

  }  // namespace bindings
}  // namespace other
