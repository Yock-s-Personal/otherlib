/**
 * \file services/editor_camera.cpp
 **/
#include "services/editor_camera.hpp"

#include <algorithm>
#include <cmath>

#include "core/profiler.hpp"
#include "math/bounding_box.hpp"

#include "renderer/camera.hpp"

namespace other {
  namespace {

    constexpr float kMinFocusDistance = 0.05f;
    constexpr float kMaxPitch = 89.f;
    /// each notch of the wheel covers this much of the way to the focus point
    constexpr float kZoomPerNotch = 0.15f;
    constexpr float kSpeedPerNotch = 1.15f;
    constexpr float kMinSpeedScale = 0.05f;
    constexpr float kMaxSpeedScale = 20.f;
    /// how much further back than the box just fitting the camera stands when it focuses
    constexpr float kFocusMargin = 1.25f;
    constexpr float kMinFocusRadius = 0.25f;

    /// the direction as the yaw and pitch camera::adjust_look_orientation keeps, in degrees
    glm::vec2 yaw_pitch_of(const glm::vec3& direction) {
      const glm::vec3 d = glm::normalize(direction);
      return { glm::degrees(std::atan2(d.z, d.x)), glm::degrees(std::asin(std::clamp(d.y, -1.f, 1.f))) };
    }

    glm::vec3 direction_of(const glm::vec2& yaw_pitch) {
      const float yaw = glm::radians(yaw_pitch.x);
      const float pitch = glm::radians(yaw_pitch.y);
      return glm::normalize(glm::vec3{ std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch) });
    }

    /// everything the camera keeps about where it is and where it looks, kept in step
    void place(camera& cam, const glm::vec3& position, const glm::vec2& yaw_pitch) {
      cam.position = position;
      cam.euler_angles.x = yaw_pitch.x;
      cam.euler_angles.y = yaw_pitch.y;
      cam.direction = direction_of(yaw_pitch);
      cam.basis = orthonormal_basis(cam.direction, cam.world_up);
    }

    glm::vec2 turned(glm::vec2 yaw_pitch, const glm::vec2& degrees) {
      yaw_pitch.x += degrees.x;
      yaw_pitch.y = std::clamp(yaw_pitch.y - degrees.y, -kMaxPitch, kMaxPitch);
      return yaw_pitch;
    }

  }  // namespace

  glm::vec3 editor_camera_controller::focus_point(const camera& cam) const {
    return cam.position + glm::normalize(cam.direction) * focus_distance;
  }

  void editor_camera_controller::update(camera& cam, const editor_camera_input& in) {
    PROFILE_SECTION("editor_camera_controller::update");
    const bool held = (dragging == drag::LOOK && in.look_held) || (dragging == drag::PAN && in.pan_held) || (dragging == drag::ORBIT && in.orbit_held);
    if (!held) {
      dragging = drag::NONE;
      if (in.over_view) {
        dragging = in.look_held ? drag::LOOK : in.orbit_held ? drag::ORBIT :
          in.pan_held                                        ? drag::PAN :
                                                               drag::NONE;
      }
    }

    glm::vec2 yaw_pitch = yaw_pitch_of(cam.direction);
    glm::vec3 position = cam.position;
    const glm::vec3 forward = glm::normalize(cam.direction);
    /// to the right and up as the view shows them, whatever hand the camera's own basis is
    const glm::vec3 right = glm::normalize(glm::cross(forward, cam.world_up));
    const glm::vec3 up = glm::cross(right, forward);
    const glm::vec2 pointer_turn = in.pointer_delta * settings.look_sensitivity;

    switch (dragging) {
      case drag::LOOK: {
        if (in.wheel != 0.f) {
          fly_speed_scale = std::clamp(fly_speed_scale * std::pow(kSpeedPerNotch, in.wheel), kMinSpeedScale, kMaxSpeedScale);
        }
        const float speed = settings.move_speed * fly_speed_scale * (in.fast ? settings.fast_factor : 1.f);
        position += (right * in.move.x + cam.world_up * in.move.y + forward * in.move.z) * speed * in.seconds;
        yaw_pitch = turned(yaw_pitch, pointer_turn);
      } break;

      case drag::ORBIT: {
        const glm::vec3 pivot = position + forward * focus_distance;
        yaw_pitch = turned(yaw_pitch, pointer_turn);
        position = pivot - direction_of(yaw_pitch) * focus_distance;
      } break;

      case drag::PAN: {
        /// what is under the pointer at the focus point's distance stays under it
        const float per_pixel = 2.f * focus_distance * std::tan(glm::radians(cam.fov) * 0.5f) / std::max(in.view_height, 1.f);
        position += (up * in.pointer_delta.y - right * in.pointer_delta.x) * per_pixel;
      } break;

      case drag::NONE:
        if (in.over_view && in.wheel != 0.f) {
          /// towards the focus point and never through it; away from it by the same measure
          const float distance = std::max(focus_distance * std::pow(1.f - kZoomPerNotch, in.wheel), kMinFocusDistance);
          position += forward * (focus_distance - distance);
          focus_distance = distance;
        }
        break;

      default:
        break;
    }

    if (in.over_view || dragging != drag::NONE) {
      yaw_pitch = turned(yaw_pitch, in.turn_degrees);
    }
    place(cam, position, yaw_pitch);
  }

  void editor_camera_controller::focus(camera& cam, const bounding_box& box) {
    PROFILE_SECTION("editor_camera_controller::focus");
    const glm::vec3 centre = (box.min + box.max) * 0.5f;
    const float radius = std::max(glm::length(box.max - box.min) * 0.5f, kMinFocusRadius);
    /// far enough that a ball round the box is inside the view's angle
    focus_distance = std::max(kFocusMargin * radius / std::sin(glm::radians(cam.fov) * 0.5f), kMinFocusDistance);

    const glm::vec2 yaw_pitch = yaw_pitch_of(cam.direction);
    place(cam, centre - direction_of(yaw_pitch) * focus_distance, yaw_pitch);
  }

}  // namespace other
