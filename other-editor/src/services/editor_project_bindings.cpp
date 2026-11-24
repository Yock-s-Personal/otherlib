/**
 * \file services/editor_project_bindings.cpp
 * the editor's bindings about the project as a thing that is made, compiled and packaged
 **/
#include <cstring>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "driver/driver.hpp"
#include "driver/systems/project_system.hpp"
#include "scripting/main_thread_only.hpp"
#include "vm/hooks.hpp"

#include "cli/environment.hpp"
#include "cli/tools/create_project.hpp"
#include "services/editor_bindings.hpp"
#include "services/editor_services.hpp"

namespace other {
  namespace bindings {
    namespace {

      std::string_view at(const ostd::vector<std::string>& list, int32_t index) {
        return index >= 0 && static_cast<size_t>(index) < list.size() ? std::string_view{ list[index] } : std::string_view{};
      }

      std::string_view text_or_empty(const char* text) {
        return text != nullptr ? std::string_view{ text } : std::string_view{};
      }

    }  // namespace

    int32_t native_editor_compile_poll(uint64_t* out_stamp) {
      editor_services* editor = get_editor_services();
      if (editor == nullptr) {
        *out_stamp = 0;
        return static_cast<int32_t>(compile_service::state::NO_PROJECT);
      }
      const compile_service::state now = editor->compile.poll(editor->driver_ptr);
      *out_stamp = editor->compile.stamp;
      return static_cast<int32_t>(now);
    }

    void native_editor_compile_refresh() {
      if (editor_services* editor = get_editor_services(); editor != nullptr) {
        editor->compile.refresh();
      }
    }

    int32_t native_editor_compile_count(int32_t what) {
      const editor_services* editor = get_editor_services();
      if (editor == nullptr) {
        return 0;
      }
      const compile_preview& preview = editor->compile.preview;
      const opt<compile_report>& report = editor->compile.session.last_report();
      size_t count = 0;
      switch (static_cast<compile_text>(what)) {
        case compile_text::SCENES: count = preview.scenes.size(); break;
        case compile_text::PIPELINES: count = preview.pipelines.size(); break;
        case compile_text::VM_SOURCES: count = preview.vm_sources.size(); break;
        case compile_text::HOOKS: count = preview.hooks.size(); break;
        case compile_text::PROBLEMS: count = preview.problems.size(); break;
        case compile_text::LEFT_OUT: count = preview.left_out.size(); break;
        case compile_text::REPORT_ERRORS: count = report.has_value() ? report->errors.size() : 0; break;
        case compile_text::REPORT_WARNINGS: count = report.has_value() ? report->warnings.size() : 0; break;
        case compile_text::REPORT_LEFT_OUT: count = report.has_value() ? report->left_out.size() : 0; break;
        default: count = 1; break;
      }
      return static_cast<int32_t>(count);
    }

    const char* native_editor_compile_text(int32_t what, int32_t index) {
      const editor_services* editor = get_editor_services();
      if (editor == nullptr) {
        return returned_string({});
      }
      const compile_service& compile = editor->compile;
      const compile_preview& preview = compile.preview;
      const opt<compile_report>& report = compile.session.last_report();
      static const ostd::vector<std::string> kNone = {};

      switch (static_cast<compile_text>(what)) {
        case compile_text::PROJECT_FILE: return returned_string(compile.shown_project.filename().string());
        case compile_text::PUBLIC_NAME: return returned_string(preview.public_name);
        case compile_text::OUTPUT_FOLDER: return returned_string(compile.default_output);
        case compile_text::STARTING_SCENE: return returned_string(preview.starting_scene);
        case compile_text::MAIN_PIPELINE: return returned_string(preview.main_pipeline);
        case compile_text::SCENES: return returned_string(at(preview.scenes, index));
        case compile_text::PIPELINES: return returned_string(at(preview.pipelines, index));
        case compile_text::VM_SOURCES: return returned_string(at(preview.vm_sources, index));

        case compile_text::HOOKS: {
          if (index < 0 || static_cast<size_t>(index) >= preview.hooks.size()) {
            return returned_string({});
          }
          const project_hook_handler& handler = preview.hooks[index];
          return returned_string(std::format("{}: {} in {}", hook_name(handler.hook), handler.block, handler.unit));
        }

        case compile_text::PROBLEMS: return returned_string(at(preview.problems, index));
        case compile_text::LEFT_OUT: return returned_string(at(preview.left_out, index));
        case compile_text::LEFT_OUT_SIZE: return returned_string(readable_size(preview.left_out_bytes));
        case compile_text::REPORT_PACKAGE: return returned_string(report.has_value() ? report->package_root.string() : std::string{});
        case compile_text::REPORT_ERRORS: return returned_string(at(report.has_value() ? report->errors : kNone, index));
        case compile_text::REPORT_WARNINGS: return returned_string(at(report.has_value() ? report->warnings : kNone, index));
        case compile_text::REPORT_LEFT_OUT: return returned_string(at(report.has_value() ? report->left_out : kNone, index));
        case compile_text::REPORT_LEFT_OUT_SIZE: return returned_string(readable_size(report.has_value() ? report->left_out_bytes : 0));
        default: return returned_string({});
      }
    }

    const char* native_editor_compile_name_problem(const char* public_name) {
      const opt<std::string> reason = invalid_public_name_reason(text_or_empty(public_name));
      return returned_string(reason.has_value() ? std::string_view{ *reason } : std::string_view{});
    }

    const char* native_editor_compile_start(const char* public_name, const char* output_folder, const char* starting_scene, const char* main_pipeline, nbool32 debug) {
      PROFILE_SECTION("bindings::native_editor_compile_start");
      editor_services* editor = get_editor_services();
      if (editor == nullptr) {
        return returned_string("There is no editor to compile with.");
      }
      return returned_string(editor->compile.start(editor->driver_ptr, text_or_empty(public_name), text_or_empty(output_folder), text_or_empty(starting_scene), text_or_empty(main_pipeline), debug != 0));
    }

    nbool32 native_editor_compile_progress(int32_t* out_stage, int32_t* out_stages, nbool32* out_cancelling) {
      const editor_services* editor = get_editor_services();
      const bool running = editor != nullptr && editor->compile.session.running();
      *out_stages = static_cast<int32_t>(compile_stage::NUM_STAGES);
      *out_stage = running ? static_cast<int32_t>(editor->compile.session.fraction() * static_cast<float>(compile_stage::NUM_STAGES) + 0.5f) : 0;
      *out_cancelling = running && editor->compile.session.cancelling();
      return running;
    }

    const char* native_editor_compile_stage_label() {
      const editor_services* editor = get_editor_services();
      return returned_string(editor != nullptr && editor->compile.session.running() ? editor->compile.session.stage_label() : std::string_view{});
    }

    void native_editor_compile_cancel() {
      if (editor_services* editor = get_editor_services(); editor != nullptr && editor->compile.session.running()) {
        editor->compile.session.cancel();
      }
    }

    int32_t native_editor_compile_result() {
      const editor_services* editor = get_editor_services();
      return static_cast<int32_t>(editor != nullptr ? editor->compile.last_result() : compile_service::result::NONE);
    }

    nbool32 native_editor_pick_folder() {
      editor_services* editor = get_editor_services();
      return editor != nullptr && editor->pick_folder();
    }

    nbool32 native_editor_take_picked_folder(char* out_path, int32_t capacity) {
      editor_services* editor = get_editor_services();
      if (editor == nullptr || out_path == nullptr || capacity <= 0) {
        return false;
      }
      const opt<std::string> picked = editor->take_picked_folder();
      if (!picked.has_value() || picked->size() >= static_cast<size_t>(capacity)) {
        return false;
      }
      std::memcpy(out_path, picked->c_str(), picked->size() + 1);
      return true;
    }

    const char* native_editor_create_project(const char* name, const char* folder, const char* author, const char* description) {
      PROFILE_SECTION("bindings::native_editor_create_project");
      editor_services* editor = get_editor_services();
      if (editor == nullptr) {
        return returned_string("There is no editor to make a project with.");
      }

      cli::new_project request = { .name = std::string{ text_or_empty(name) }, .directory = filepath{ text_or_empty(folder) } };
      if (const std::string_view given = text_or_empty(author); !given.empty()) {
        request.author = given;
      }
      if (const std::string_view given = text_or_empty(description); !given.empty()) {
        request.description = given;
      }
      /// the environment this editor is of: its assemblies are what the new C# project names
      const cli::new_project_result made = cli::create_project(request, cli::locate_environment());
      if (made.error.has_value()) {
        return returned_string(*made.error);
      }

      CORE_LOG_INFO("Created project '{}' as '{}' ({} files{}).", request.name, made.project_file.string(), made.created.size(),
                    made.with_cs_project ? "" : ", with no C# script project: no environment was found to wire it to");
      editor->created_project = made.project_file.string();
      /// a project that is open stays open: the engine finishes closing one only as it shuts
      ///  down, so there is no opening another over it. and a project's paths are written
      ///  from the folder that holds it, so it opens only from there: when that folder is
      ///  the one this editor works from
      std::error_code ec;
      const bool made_here = std::filesystem::equivalent(request.directory, std::filesystem::current_path(ec), ec) && !ec;
      editor->created_project_opening = made_here && editor->driver_ptr != nullptr && editor->driver_ptr->get_kernel().get_core_system<project_system>().is_project_empty();
      if (editor->created_project_opening) {
        editor->driver_ptr->queue_project_load(made.project_file);
      }
      return returned_string({});
    }

    nbool32 native_editor_created_project_opening() {
      const editor_services* editor = get_editor_services();
      return editor != nullptr && editor->created_project_opening;
    }

    const char* native_editor_created_project() {
      const editor_services* editor = get_editor_services();
      return returned_string(editor != nullptr ? std::string_view{ editor->created_project } : std::string_view{});
    }

  }  // namespace bindings
}  // namespace other
