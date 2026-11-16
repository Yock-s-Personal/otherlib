/**
 * \file ui/project-overview/project_overview.cpp
 **/
#include "ui/project-overview/project_overview.hpp"

#include <algorithm>
#include <filesystem>
#include <format>
#include <string_view>

#include <imgui/imgui.h>

#include "core/profiler.hpp"

#include "theme/colors.hpp"
#include "ui/ui_helpers.hpp"

#include "driver/driver.hpp"
#include "driver/systems/project_system.hpp"
#include "ui/inspector_widgets.hpp"

namespace other {
  namespace ui {
    namespace {

      constexpr ImVec2 kDefaultSize{ 520.f, 620.f };
      constexpr float kDetailListHeight = 160.f;

      std::string_view state_label(project::state state) {
        switch (state) {
          case project::EMPTY: return "Empty";
          case project::LOADING: return "Loading";
          case project::LOADED: return "Loaded";
          case project::LOAD_FAILED: return "Load failed";
          case project::UNLOADING: return "Unloading";
          default: return "Unknown";
        }
      }

      /// msbuild starts each diagnostic with its node and project number ("  1:7>"), writes the
      ///  source as an absolute path and ends with " [<csproj>]"; all three repeat on every
      ///  line and push the message itself out of a narrow panel
      std::string tidy_build_line(std::string_view line, const std::string_view project_dir) {
        const size_t first = line.find_first_not_of(" \t");
        line.remove_prefix(first == std::string_view::npos ? line.size() : first);
        if (const size_t marker = line.find('>');
            marker != std::string_view::npos && line.substr(0, marker).find_first_not_of("0123456789:") == std::string_view::npos) {
          line.remove_prefix(marker + 1);
        }

        if (line.ends_with(']')) {
          if (const size_t open = line.rfind(" ["); open != std::string_view::npos) {
            line = line.substr(0, open);
          }
        }
        if (!project_dir.empty() && line.starts_with(project_dir)) {
          line.remove_prefix(project_dir.size());
        }
        return std::string{ line };
      }

    }  // namespace

    project_overview::project_overview(event_system& events, driver* drvr)
        : ui_window(&events, "Project"), driver_ptr(drvr) {
    }

    void project_overview::on_prepare_render() {
      /// first use only: a size saved in the layout wins after that
      ImGui::SetNextWindowSize(kDefaultSize, ImGuiCond_FirstUseEver);
    }

    void project_overview::on_render_body() {
      PROFILE_SECTION("project_overview::on_render_body");
      auto& projects = driver_ptr->get_kernel().get_core_system<project_system>();
      const project& current = projects.get_project();

      if (current.is_empty()) {
        ImGui::TextDisabled("No project is open.");
        return;
      }

      render_metadata(projects, current);
      render_scripts(projects, current);
      render_load_errors(current);
    }

    void project_overview::render_metadata(project_system& projects, const project& current) {
      const project::metadata& meta = current.get_metadata();

      ImGui::SeparatorText("Project");
      inspector::property_display("Name", meta.name);
      inspector::property_display("Version", meta.version);
      inspector::property_display("Author", meta.author);
      inspector::property_display("Description", meta.description);
      if (const opt<filepath>& file = projects.get_project_file(); file.has_value()) {
        inspector::property_display("File", file->string());
      }
      inspector::property_display("State", state_label(current.get_state()));

      const auto scenes = current.get_scenes();
      inspector::property_display("Scenes", std::format("{}", scenes.size()));

      /// a dropped starting scene leaves id 0: nothing is activated until the user picks one
      const natural_t start_id = current.get_starting_scene_id();
      const auto start = std::ranges::find(scenes, start_id, &project::scene::scene_id);
      if (start_id != 0 && start != scenes.end()) {
        inspector::property_display("Start scene", start->name);
      } else {
        inspector::property_display("Start scene", "none", colors::kFriendlyErrorRed);
      }
    }

    void project_overview::render_scripts(project_system& projects, const project& current) {
      ImGui::SeparatorText("C# Scripts");

      const opt<script_failure>& failure = current.get_script_failure();
      const filepath csproj = current.script_project_path();
      if (csproj.empty() && !failure.has_value()) {
        ImGui::TextDisabled("This project has no C# project.");
        return;
      }

      const bool building = projects.script_project_building();
      const bool mounted = current.script_project_mounted();
      inspector::property_display("Project", csproj.string());

      if (building) {
        inspector::property_display("Status", "Building...");
      } else if (failure.has_value() && mounted) {
        /// a rebuild failed after load: the assembly from the last good build is still running
        inspector::property_display("Status", "Build failed, running the last good build", colors::kFriendlyErrorRed);
      } else if (failure.has_value()) {
        inspector::property_display("Status", "Not loaded", colors::kFriendlyErrorRed);
      } else if (mounted) {
        inspector::property_display("Status", "Loaded");
      } else {
        inspector::property_display("Status", "Not loaded");
      }

      const bool can_rebuild = !building && current.is_loaded() && (!failure.has_value() || failure->can_retry);
      ImGui::BeginDisabled(!can_rebuild);
      if (ImGui::Button(failure.has_value() ? "Retry build" : "Rebuild")) {
        projects.request_script_rebuild();
      }
      ImGui::EndDisabled();

      if (!failure.has_value()) {
        return;
      }

      {
        scoped_color error_color{ ImGuiCol_Text, colors::rgba_to_imvec4(colors::kFriendlyErrorRed) };
        ImGui::TextWrapped("%s", failure->reason.c_str());
      }
      if (!failure->can_retry) {
        ImGui::TextWrapped("Restore the file and reopen the project.");
        return;
      }

      refresh_detail_lines(failure->details, failure->csproj_path);
      if (detail_lines.empty()) {
        ImGui::TextDisabled("Nothing more was captured. The editor log has the full output.");
        return;
      }

      if (ImGui::SmallButton("Copy")) {
        ImGui::SetClipboardText(failure->details.c_str());
      }
      if (ImGui::BeginChild("##script-failure-details", ImVec2(0.f, kDetailListHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY)) {
        for (const std::string& line : detail_lines) {
          ImGui::TextWrapped("%s", line.c_str());
        }
      }
      ImGui::EndChild();
    }

    void project_overview::render_load_errors(const project& current) {
      const auto errors = current.get_load_errors();
      if (errors.empty()) {
        return;
      }

      ImGui::SeparatorText("Load Errors");
      for (const load_error& error : errors) {
        ImGui::Bullet();
        ImGui::TextWrapped("%s: %s", error.subject.c_str(), error.message.c_str());
      }
    }

    void project_overview::refresh_detail_lines(const std::string& details, const filepath& csproj) {
      if (details == shown_details) {
        return;
      }
      PROFILE_SECTION("project_overview::refresh_detail_lines");
      shown_details = details;
      detail_lines.clear();

      std::error_code ec;
      filepath dir = std::filesystem::absolute(csproj, ec).parent_path();
      const std::string project_dir = ec ? std::string{} : dir.make_preferred().string() + static_cast<char>(filepath::preferred_separator);

      std::string_view rest = details;
      while (!rest.empty()) {
        const size_t end = rest.find('\n');
        std::string_view line = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);

        if (line.ends_with('\r')) {
          line.remove_suffix(1);
        }

        std::string tidy = tidy_build_line(line, project_dir);
        if (!tidy.empty()) {
          detail_lines.push_back(std::move(tidy));
        }
      }
    }

  }  // namespace ui
}  // namespace other