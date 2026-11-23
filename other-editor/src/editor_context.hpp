/**
 * \file editor_context.hpp
 **/
#ifndef OTHER_EDITOR_EDITOR_CONTEXT_HPP
#define OTHER_EDITOR_EDITOR_CONTEXT_HPP

#include <imgui/imgui.h>
#include <imguizmo/ImGuizmo.h>

#include "renderer/camera.hpp"

#include "scene/scene.hpp"

#include "project/compile_session.hpp"

#include "editor_settings.hpp"
#include "services/editor_services.hpp"

namespace other {

  class editor_driver;

  /// the selection and the edits that can be undone are the services'; what is here is the
  ///  driver's own: its viewports, its camera, its compile
  struct editor_context : editor_services {
    struct viewport_info {
      natural_t vp_id;
      std::string viewport_name;
    };

    editor_driver* driver;
    editor_settings settings;

    scene* active_scene = nullptr;
    camera* scene_active_camera = nullptr;
    natural_t scene_viewport_handle = 0;

    /// requested by ui, applied in editor_driver::update_running so the active scene
    ///  never changes while a frame is being rendered

    camera editor_camera;
    ImGuizmo::MODE gizmo_mode = ImGuizmo::MODE::LOCAL;
    ImGuizmo::OPERATION gizmo_operation = ImGuizmo::OPERATION::TRANSLATE;

    std::string current_primary_viewport = "default-instancing";
    ostd::vector<viewport_info> viewports;

    natural_t register_viewport(const std::string_view name, const std::string_view render_pipeline_name);
    natural_t register_viewport(const std::string_view name, const std::string_view render_pipeline_name, camera* cam);
    void remove_viewport(natural_t vp_id);
    void remove_all_viewports();

    viewport& get_viewport(natural_t vp_id);

    ImTextureID get_texture_id_by_name(const std::string_view name) const;
    ImTextureID get_pipeline_texture_id(const std::string_view pipeline_name, const std::string_view texture_name) const;

    ImVec2 get_pipeline_texture_size(const std::string_view pipeline_name, const std::string_view texture_name) const;

    /// the compile the compile window starts. it is here and not in the window so the driver
    ///  can wait for it at shutdown, before the VM whose devices it links against is gone
    compile_session project_compile;
  };

}  // namespace other

#endif  // OTHER_EDITOR_EDITOR_CONTEXT_HPP