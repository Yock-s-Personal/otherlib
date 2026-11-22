/**
 * \file ui/project-compiler/project_compiler.cpp
 **/
#include "ui/project-compiler/project_compiler.hpp"

#include <algorithm>
#include <filesystem>
#include <format>

#include <imgui/imgui.h>

#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "driver/driver.hpp"
#include "driver/systems/project_system.hpp"
#include "driver/systems/rendering_system.hpp"
#include "theme/colors.hpp"
#include "ui/inspector_widgets.hpp"
#include "ui/ui_helpers.hpp"
#include "vm/hooks.hpp"
#include "vm/other_device.hpp"
#include "vm/vm.hpp"

#include "editor_context.hpp"
#include "editor_driver.hpp"

namespace other {
  namespace ui {
    namespace {

      constexpr ImVec2 kDefaultSize{ 560.f, 680.f };
      constexpr float kListHeight = 110.f;
      constexpr const char* kUnsavedPrompt = "Unsaved Scene";

      /// the bus the running VM links against, so a package's boot program is linked against
      ///  the same devices it will find; null before the VM is up
      const command_bus* live_bus() {
        const other_command_device* device = vm::get_initialized_device();
        return device == nullptr ? nullptr : device->bus;
      }

      void copy_into(std::span<char> buffer, const std::string_view text) {
        const size_t count = std::min(text.size(), buffer.size() - 1);
        std::copy_n(text.begin(), count, buffer.begin());
        buffer[count] = '\0';
      }

      /// a combo over names; true when the selection changed
      bool name_combo(const char* id, std::span<const std::string> names, std::string& selected) {
        bool changed = false;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo(id, selected.empty() ? "choose one" : selected.c_str())) {
          for (const std::string& name : names) {
            if (ImGui::Selectable(name.c_str(), name == selected) && name != selected) {
              selected = name;
              changed = true;
            }
          }
          ImGui::EndCombo();
        }
        return changed;
      }

      void error_text(const std::string_view text) {
        const scoped_color error_color{ ImGuiCol_Text, colors::rgba_to_imvec4(colors::kFriendlyErrorRed) };
        ImGui::TextWrapped("%.*s", static_cast<int>(text.size()), text.data());
      }

    }  // namespace

    project_compiler::project_compiler(editor_context& ctx, event_system& events, editor_driver* drvr)
        : ui_window(&events, "Compile Project"), editor_ctx(ctx), driver_ptr(drvr) {
    }

    void project_compiler::on_prepare_render() {
      /// first use only: a size saved in the layout wins after that
      ImGui::SetNextWindowSize(kDefaultSize, ImGuiCond_FirstUseEver);
    }

    void project_compiler::on_render_body() {
      PROFILE_SECTION("project_compiler::on_render_body");
      auto& projects = driver_ptr->get_kernel().get_core_system<project_system>();
      const project& current = projects.get_project();
      const opt<filepath>& project_file = projects.get_project_file();

      if (current.is_empty() || !project_file.has_value()) {
        ImGui::TextDisabled("No project is open.");
        return;
      }
      if (!current.is_loaded()) {
        ImGui::TextDisabled("The project is still loading.");
        return;
      }

      editor_ctx.project_compile.poll();
      take_picked_folder();
      if (*project_file != shown_project) {
        refresh(*project_file);
      }

      render_choices();
      render_contents();
      render_compile();
      render_result();
    }

    void project_compiler::refresh(const filepath& project_file) {
      PROFILE_SECTION("project_compiler::refresh");
      const bool another_project = project_file != shown_project;
      shown_project = project_file;

      const command_bus* bus = live_bus();
      if (bus == nullptr) {
        preview = {};
        preview.problems.push_back("the VM is not running, so a boot program cannot be linked");
        return;
      }
      preview = preview_project_compile(*bus, shown_project);
      CORE_LOG_DEBUG("[COMPILE] : '{}' would link {} VM source(s) with {} hook handler(s) and draw with {} pipeline(s)", shown_project.string(), preview.vm_sources.size(),
                     preview.hooks.size(), preview.pipelines.size());

      /// what was typed for one project is not carried to the next; a refresh of the same
      ///  one keeps it
      if (another_project) {
        copy_into(name_buf, preview.public_name);
        std::error_code ec;
        const filepath project_folder = std::filesystem::absolute(shown_project, ec).parent_path();
        copy_into(output_buf, (project_folder.parent_path() / "packages").string());
      }
      if (std::ranges::find(preview.scenes, starting_scene) == preview.scenes.end()) {
        starting_scene = preview.starting_scene;
      }
      if (std::ranges::find(preview.pipelines, main_pipeline) == preview.pipelines.end()) {
        main_pipeline = preview.main_pipeline;
      }
    }

    void project_compiler::on_folder_picked(void* userdata, const char* const* filelist, int32_t filter) {
      /// null or empty when the dialog was cancelled
      if (userdata == nullptr || filelist == nullptr || filelist[0] == nullptr) {
        return;
      }
      auto* window = static_cast<project_compiler*>(userdata);
      const std::lock_guard lock(window->picked_mutex);
      window->picked_folder = std::string{ filelist[0] };
    }

    void project_compiler::take_picked_folder() {
      const std::lock_guard lock(picked_mutex);
      if (picked_folder.has_value()) {
        copy_into(output_buf, *picked_folder);
        picked_folder.reset();
      }
    }

    std::string project_compiler::blocking_reason() const {
      if (!preview.problems.empty()) {
        return "The project has problems that stop a compile.";
      }
      if (const opt<std::string> reason = invalid_public_name_reason(name_buf.data()); reason.has_value()) {
        return std::format("The public name cannot be used: {}.", *reason);
      }
      if (output_buf[0] == '\0') {
        return "Choose an output folder.";
      }
      if (starting_scene.empty()) {
        return "Choose the scene the game starts in.";
      }
      if (main_pipeline.empty()) {
        return "Choose the pipeline whose image reaches the window.";
      }
      return {};
    }

    void project_compiler::request_compile() {
      start_error.clear();
      /// a package is made from the files on disk, so a scene with edits that are only in
      ///  memory is saved first or nothing is compiled
      if (editor_ctx.has_unsaved_scene_edits()) {
        ImGui::OpenPopup(kUnsavedPrompt);
      } else {
        start_compile();
      }
    }

    void project_compiler::start_compile() {
      PROFILE_SECTION("project_compiler::start_compile");
      start_error.clear();
      const command_bus* bus = live_bus();
      if (bus == nullptr) {
        start_error = "The VM is not running, so a boot program cannot be linked.";
        return;
      }

      /// what was chosen here is the project's choice from now on. a main pipeline is only a
      ///  choice when there are several
      const compile_choices choices = {
        .public_name = name_buf.data(),
        .starting_scene = starting_scene,
        .main_pipeline = preview.pipelines.size() > 1 ? main_pipeline : std::string{},
      };
      if (const opt<std::string> error = save_compile_choices(shown_project, choices); error.has_value()) {
        start_error = *error;
        return;
      }

      compile_settings settings = {
        .project_file = shown_project,
        .output_directory = filepath{ output_buf.data() },
        .public_name = std::string{ name_buf.data() },
        .starting_scene = starting_scene,
        .main_pipeline = main_pipeline,
        .config = debug_package ? package_config::DEBUG : package_config::RELEASE,
      };
      if (!editor_ctx.project_compile.start(driver_ptr->get_job_system(), *bus, std::move(settings))) {
        start_error = "A compile is already running.";
      }
    }

    void project_compiler::render_choices() {
      ImGui::SeparatorText("Package");
      /// the file's name: the field is too narrow for a path
      inspector::property_display("Project", shown_project.filename().string());
      inspector::property_text("Public name", name_buf.data(), static_cast<uint32_t>(name_buf.size()));

      inspector::begin_property_row("Output folder");
      ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Browse...").x - ImGui::GetStyle().FramePadding.x * 4.f);
      ImGui::InputText("##output-folder", output_buf.data(), output_buf.size());
      ImGui::SameLine();
      if (ImGui::Button("Browse...")) {
        driver_ptr->get_kernel().get_core_system<rendering_system>().show_open_folder_dialog(&project_compiler::on_folder_picked, this, 0);
      }
      inspector::end_property_row();

      inspector::begin_property_row("Configuration");
      if (ImGui::RadioButton("Release", !debug_package)) {
        debug_package = false;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Debug", debug_package)) {
        debug_package = true;
      }
      inspector::end_property_row();

      inspector::begin_property_row("Starting scene");
      name_combo("##starting-scene", preview.scenes, starting_scene);
      inspector::end_property_row();

      /// with one pipeline there is nothing to choose
      if (preview.pipelines.size() > 1) {
        inspector::begin_property_row("Main pipeline");
        name_combo("##main-pipeline", preview.pipelines, main_pipeline);
        inspector::end_property_row();
      } else {
        inspector::property_display("Main pipeline", main_pipeline.empty() ? "none" : main_pipeline);
      }
    }

    void project_compiler::render_contents() {
      ImGui::SeparatorText("Boot Program");
      if (preview.vm_sources.empty()) {
        ImGui::TextDisabled("The project has no VM sources. The boot program only loads the project.");
      } else if (ImGui::BeginChild("##vm-sources", ImVec2(0.f, kListHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY)) {
        for (const std::string& source : preview.vm_sources) {
          ImGui::BulletText("%s", source.c_str());
        }
        for (const project_hook_handler& handler : preview.hooks) {
          const std::string_view hook = hook_name(handler.hook);
          ImGui::BulletText("%.*s: %s in %s", static_cast<int>(hook.size()), hook.data(), handler.block.c_str(), handler.unit.c_str());
        }
      }
      if (!preview.vm_sources.empty()) {
        ImGui::EndChild();
      }

      for (const std::string& problem : preview.problems) {
        ImGui::Bullet();
        error_text(problem);
      }
      if (ImGui::SmallButton("Refresh")) {
        refresh(shown_project);
      }
    }

    void project_compiler::render_compile() {
      ImGui::SeparatorText("Compile");
      const std::string reason = blocking_reason();
      const bool running = editor_ctx.project_compile.running();
      ImGui::BeginDisabled(running || !reason.empty());
      if (ImGui::Button(running ? "Compiling..." : "Compile")) {
        request_compile();
      }
      ImGui::EndDisabled();
      render_unsaved_prompt();

      if (!running && !reason.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", reason.c_str());
      }
      if (!start_error.empty()) {
        error_text(start_error);
      }
    }

    void project_compiler::render_unsaved_prompt() {
      if (!ImGui::BeginPopupModal(kUnsavedPrompt, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
      }

      ImGui::TextUnformatted("The active scene has edits that are not saved.");
      ImGui::TextUnformatted("A package is made from the files on disk.");
      ImGui::Spacing();
      if (ImGui::Button("Save and compile")) {
        ImGui::CloseCurrentPopup();
        if (driver_ptr->save_active_scene()) {
          start_compile();
        } else {
          start_error = "The active scene could not be saved, so nothing was compiled. The log says why.";
        }
      }
      ImGui::SameLine();
      /// declining the save is declining the compile
      if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    void project_compiler::render_result() {
      const opt<compile_report>& report = editor_ctx.project_compile.last_report();
      if (!report.has_value()) {
        return;
      }

      ImGui::SeparatorText("Result");
      if (report->success) {
        const std::string package = report->package_root.string();
        ImGui::TextWrapped("Packaged as %s", package.c_str());
        if (ImGui::SmallButton("Copy path")) {
          ImGui::SetClipboardText(package.c_str());
        }
        if (!report->left_out.empty()) {
          const std::string left_out = std::format("{} file(s) that nothing in the project uses were left out ({}). Name the ones the game loads by path in [package] keep.",
                                                   report->left_out.size(), readable_size(report->left_out_bytes));
          ImGui::TextWrapped("%s", left_out.c_str());
          if (ImGui::BeginChild("##left-out", ImVec2(0.f, kListHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY)) {
            for (const std::string& name : report->left_out) {
              ImGui::TextUnformatted(name.c_str());
            }
          }
          ImGui::EndChild();
        }
      } else {
        error_text("The project was not packaged.");
      }
      for (const std::string& error : report->errors) {
        ImGui::Bullet();
        error_text(error);
      }
      for (const std::string& warning : report->warnings) {
        ImGui::Bullet();
        ImGui::TextWrapped("%s", warning.c_str());
      }
    }

  }  // namespace ui
}  // namespace other
