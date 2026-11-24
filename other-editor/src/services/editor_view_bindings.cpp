/**
 * \file services/editor_view_bindings.cpp
 * the editor's view of the scene as Other.Editor.SceneViewport asks about it
 **/
#include "core/profiler.hpp"

#include "renderer/camera.hpp"

#include "services/editor_bindings.hpp"
#include "services/editor_services.hpp"
#include "services/scene_pick.hpp"

namespace other {
  namespace bindings {
    namespace {

      constexpr uint32_t kViewShown = 1u << 0;
      constexpr uint32_t kViewHovered = 1u << 1;
      constexpr uint32_t kViewClicked = 1u << 2;

      /// the services, when there is a view with a camera and a picture to ask about
      editor_services* viewed() {
        editor_services* editor = get_editor_services();
        if (editor == nullptr || editor->view_camera == nullptr || editor->view.size.x < 1.f || editor->view.size.y < 1.f) {
          return nullptr;
        }
        return editor;
      }

    }  // namespace

    nbool32 native_editor_view_camera(float* position, float* direction, float* focus_point) {
      editor_services* editor = get_editor_services();
      if (editor == nullptr || editor->view_camera == nullptr || position == nullptr || direction == nullptr || focus_point == nullptr) {
        return false;
      }
      const camera& cam = *editor->view_camera;
      const glm::vec3 focus = editor->camera_controller.focus_point(cam);
      for (int32_t i = 0; i < 3; ++i) {
        position[i] = cam.position[i];
        direction[i] = cam.direction[i];
        focus_point[i] = focus[i];
      }
      return true;
    }

    nbool32 native_editor_view_focus_selection() {
      editor_services* editor = get_editor_services();
      return editor != nullptr && editor->focus_view_on_selection();
    }

    uint32_t native_editor_view_state(float* rectangle) {
      const editor_services* editor = get_editor_services();
      if (editor == nullptr || rectangle == nullptr) {
        return 0;
      }
      const editor_services::scene_view_state& view = editor->view;
      rectangle[0] = view.position.x;
      rectangle[1] = view.position.y;
      rectangle[2] = view.size.x;
      rectangle[3] = view.size.y;
      return (view.shown ? kViewShown : 0u) | (view.hovered ? kViewHovered : 0u) | (view.clicked ? kViewClicked : 0u);
    }

    nbool32 native_editor_view_ray(float x, float y, float* origin, float* direction) {
      const editor_services* editor = viewed();
      if (editor == nullptr || origin == nullptr || direction == nullptr) {
        return false;
      }
      const pick_ray r = view_ray(*editor->view_camera, editor->view.size, glm::vec2{ x, y } - editor->view.position);
      for (int32_t i = 0; i < 3; ++i) {
        origin[i] = r.origin[i];
        direction[i] = r.direction[i];
      }
      return true;
    }

    nbool32 native_editor_view_place(float x, float y, float z, float* place) {
      const editor_services* editor = viewed();
      if (editor == nullptr || place == nullptr) {
        return false;
      }
      const opt<glm::vec2> shown = view_place(*editor->view_camera, editor->view.size, { x, y, z });
      if (!shown.has_value()) {
        return false;
      }
      place[0] = editor->view.position.x + shown->x;
      place[1] = editor->view.position.y + shown->y;
      return true;
    }

    float native_editor_view_pixel_size(float x, float y, float z) {
      const editor_services* editor = viewed();
      return editor != nullptr ? view_units_per_pixel(*editor->view_camera, editor->view.size, { x, y, z }) : 0.f;
    }

    uint64_t native_editor_view_pick(float x, float y) {
      PROFILE_SECTION("bindings::native_editor_view_pick");
      editor_services* editor = viewed();
      return editor != nullptr ? editor->pick_in_view({ x, y }) : 0u;
    }

    nbool32 native_editor_view_grid(int32_t shown) {
      editor_services* editor = get_editor_services();
      if (editor == nullptr) {
        return false;
      }
      if (shown == 0 || shown == 1) {
        editor->view.grid = shown == 1;
      }
      return editor->view.grid;
    }

    uint64_t native_editor_view_place_model(const char* path, float x, float y) {
      editor_services* editor = viewed();
      return editor != nullptr && path != nullptr ? editor->place_model(path, { x, y }) : 0u;
    }

  }  // namespace bindings
}  // namespace other
