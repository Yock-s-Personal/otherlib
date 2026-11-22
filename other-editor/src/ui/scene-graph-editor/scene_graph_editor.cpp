/**
 * \file ui/scene-graph-editor/scene_graph_editor.cpp
 **/
#include "ui/scene-graph-editor/scene_graph_editor.hpp"

#include <format>

#include <imgui/imgui.h>

#include "core/profiler.hpp"

#include "driver/driver.hpp"
#include "driver/systems/project_system.hpp"
#include "driver/systems/scene_system.hpp"

namespace other {
  namespace ui {

    scene_graph_editor::scene_graph_editor(editor_context& ctx, event_system& events, driver* drvr)
        : ui_window(&events, "Scene Graph"), editor_ctx(ctx), driver_ptr(drvr) {
    }

    void scene_graph_editor::on_render_body() {
      PROFILE_SECTION("scene_graph_editor::on_render_body");
      auto& kernel = driver_ptr->get_kernel();
      auto& scenes = kernel.get_core_system<scene_system>();
      auto& projects = kernel.get_core_system<project_system>();

      const project& current_project = projects.get_project();
      const scene* active_scene = scenes.get_active_scene();
      const natural_t starting_scene_id = current_project.get_starting_scene_id();

      /// a scene can only be activated once its document has loaded
      const bool project_loading = projects.is_project_loading();
      if (project_loading) {
        ImGui::TextDisabled("Project is loading...");
      }

      ImGui::BeginDisabled(project_loading);

      uint32_t listed = 0;
      for (const auto& node : scenes.get_scene_graph()) {
        if (node.value == nullptr) {
          continue;
        }
        const scene& s = *node.value;
        ++listed;

        /// the project names a scene separately from its file stem; show both when they differ
        std::string label = s.name;
        for (const project::scene& entry : current_project.get_scenes()) {
          if (entry.scene_id == s.id && entry.name != s.name) {
            label = std::format("{} ({})", entry.name, s.name);
            break;
          }
        }

        const bool is_active = active_scene != nullptr && active_scene->id == s.id;
        if (is_active) {
          label += "  [active]";
        }
        if (starting_scene_id != 0 && starting_scene_id == s.id) {
          label += "  [start]";
        }

        /// the id after ### keeps the row stable while the tags in the label change
        label += std::format("###scene-{}", s.id);
        if (ImGui::Selectable(label.c_str(), is_active) && !is_active) {
          editor_ctx.pending_scene_activation = s.id;
        }
      }

      ImGui::EndDisabled();

      if (listed == 0) {
        ImGui::TextDisabled("No scenes in the scene graph.");
      }
    }

  }  // namespace ui
}  // namespace other
