/**
 * \file services/scene_handles.cpp
 **/
#include "services/scene_handles.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/profiler.hpp"

#include "renderer/camera.hpp"

#include "services/scene_pick.hpp"

namespace other {
  namespace {

    constexpr float kParallel = 1e-4f;
    constexpr float kSmallestFactor = 0.01f;
    constexpr int32_t kRingSegments = 48;
    /// how many of the picture's units of drag double the size, for the round handle
    constexpr float kPixelsToDouble = 100.f;
    constexpr float kTwoPi = 2.f * std::numbers::pi_v<float>;

    float snapped(float value, float snap) {
      return snap > 0.f ? std::round(value / snap) * snap : value;
    }

    float distance_to_segment(const glm::vec2& point, const glm::vec2& a, const glm::vec2& b) {
      const glm::vec2 along = b - a;
      const float length_squared = glm::dot(along, along);
      if (length_squared <= 0.f) {
        return glm::distance(point, a);
      }
      const float t = std::clamp(glm::dot(point - a, along) / length_squared, 0.f, 1.f);
      return glm::distance(point, a + along * t);
    }

    /// which side of the line from a to b the point is on
    float side(const glm::vec2& point, const glm::vec2& a, const glm::vec2& b) {
      return (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
    }

    /// how far along a line, from a point of it, is the place on it nearest the ray. none
    ///  when the ray runs along the line, which has no nearest place
    opt<float> along_axis(const pick_ray& r, const glm::vec3& point, const glm::vec3& axis) {
      const glm::vec3 between = point - r.origin;
      const float b = glm::dot(axis, r.direction);
      const float denominator = 1.f - b * b;
      if (denominator < kParallel) {
        return std::nullopt;
      }
      return (b * glm::dot(r.direction, between) - glm::dot(axis, between)) / denominator;
    }

    opt<glm::vec3> on_plane(const pick_ray& r, const glm::vec3& point, const glm::vec3& normal) {
      const float towards = glm::dot(r.direction, normal);
      if (std::abs(towards) < kParallel) {
        return std::nullopt;
      }
      const float t = glm::dot(point - r.origin, normal) / towards;
      if (t <= 0.f) {
        return std::nullopt;
      }
      return r.origin + r.direction * t;
    }

    /// two directions square to an axis and to each other, the second a quarter turn
    ///  counter-clockwise from the first seen from where the axis points
    void ring_basis(const glm::vec3& axis, glm::vec3& first, glm::vec3& second) {
      const glm::vec3 other_way = std::abs(axis.y) < 0.99f ? glm::vec3{ 0.f, 1.f, 0.f } : glm::vec3{ 1.f, 0.f, 0.f };
      first = glm::normalize(glm::cross(other_way, axis));
      second = glm::cross(axis, first);
    }

    opt<float> angle_on_ring(const pick_ray& r, const glm::vec3& centre, const glm::vec3& axis) {
      const opt<glm::vec3> hit = on_plane(r, centre, axis);
      if (!hit.has_value()) {
        return std::nullopt;
      }
      glm::vec3 first;
      glm::vec3 second;
      ring_basis(axis, first, second);
      const glm::vec3 from_centre = *hit - centre;
      return std::atan2(glm::dot(from_centre, second), glm::dot(from_centre, first));
    }

  }  // namespace

  void scene_handles::begin_frame(const handle_input& input) {
    in = input;
    asking = true;
    /// nothing else is hot while one is dragged
    hot = active != 0 ? 0 : nearest;
    nearest = 0;
    nearest_distance = kReach;
    active_asked_for = false;
    if (in.pressed) {
      taken = false;
    }
  }

  void scene_handles::end_frame() {
    asking = false;
    /// a handle nobody asked for this frame is one whose tool has gone: it is let go
    if (active != 0 && !active_asked_for) {
      active = 0;
    }
  }

  uint32_t scene_handles::track(natural_t id, opt<float> distance) {
    if (active == id) {
      active_asked_for = true;
      if (in.cancel) {
        active = 0;
        return HANDLE_ENDED | HANDLE_CANCELLED;
      }
      if (!in.down) {
        active = 0;
        return HANDLE_ENDED;
      }
      return HANDLE_ACTIVE;
    }
    if (active != 0) {
      return HANDLE_NONE;
    }

    if (in.over_view && distance.has_value() && *distance < nearest_distance) {
      nearest = id;
      nearest_distance = *distance;
    }
    if (hot != id) {
      return HANDLE_NONE;
    }
    if (in.pressed && !in.blocked) {
      active = id;
      active_asked_for = true;
      taken = true;
      start_pointer = in.pointer;
      return HANDLE_HOT | HANDLE_ACTIVE | HANDLE_STARTED;
    }
    return HANDLE_HOT;
  }

  uint32_t scene_handles::slide_axis(natural_t id, glm::vec3& position, const glm::vec3& axis, float reach, float snap) {
    PROFILE_SECTION_VERBOSE("scene_handles::slide_axis");
    if (in.cam == nullptr) {
      return HANDLE_NONE;
    }
    const opt<std::pair<glm::vec2, glm::vec2>> shown = view_segment(*in.cam, in.view_size, position, position + axis * reach);
    uint32_t flags = track(id, shown.has_value() ? opt<float>{ distance_to_segment(in.pointer, shown->first, shown->second) } : std::nullopt);

    const pick_ray r = view_ray(*in.cam, in.view_size, in.pointer);
    if ((flags & HANDLE_STARTED) != 0) {
      start_position = position;
      start_along = along_axis(r, position, axis).value_or(0.f);
      return flags;
    }
    if ((flags & HANDLE_CANCELLED) != 0) {
      position = start_position;
      return flags;
    }
    if ((flags & HANDLE_ACTIVE) != 0) {
      const opt<float> along = along_axis(r, start_position, axis);
      if (along.has_value()) {
        const glm::vec3 moved = start_position + axis * snapped(*along - start_along, snap);
        if (moved != position) {
          flags |= HANDLE_CHANGED;
        }
        position = moved;
      }
    }
    return flags;
  }

  uint32_t scene_handles::slide_plane(natural_t id, glm::vec3& position, const glm::vec3& u, const glm::vec3& v, float near_edge, float far_edge, float snap) {
    PROFILE_SECTION_VERBOSE("scene_handles::slide_plane");
    if (in.cam == nullptr) {
      return HANDLE_NONE;
    }

    /// in reach when the pointer is inside the square as it is shown, whichever way round
    const glm::vec3 corners[4] = {
      position + u * near_edge + v * near_edge,
      position + u * far_edge + v * near_edge,
      position + u * far_edge + v * far_edge,
      position + u * near_edge + v * far_edge,
    };
    opt<float> distance = std::nullopt;
    glm::vec2 shown[4];
    bool all_shown = true;
    for (int32_t i = 0; i < 4 && all_shown; ++i) {
      const opt<glm::vec2> place = view_place(*in.cam, in.view_size, corners[i]);
      all_shown = place.has_value();
      shown[i] = place.value_or(glm::vec2{ 0.f, 0.f });
    }
    if (all_shown) {
      int32_t left = 0;
      int32_t right = 0;
      for (int32_t i = 0; i < 4; ++i) {
        const float s = side(in.pointer, shown[i], shown[(i + 1) % 4]);
        left += s > 0.f ? 1 : 0;
        right += s < 0.f ? 1 : 0;
      }
      if (left == 4 || right == 4) {
        distance = 0.f;
      }
    }

    const glm::vec3 normal = glm::normalize(glm::cross(u, v));
    const pick_ray r = view_ray(*in.cam, in.view_size, in.pointer);
    uint32_t flags = track(id, distance);
    if ((flags & HANDLE_STARTED) != 0) {
      start_position = position;
      start_hit = on_plane(r, position, normal).value_or(position);
      return flags;
    }
    if ((flags & HANDLE_CANCELLED) != 0) {
      position = start_position;
      return flags;
    }
    if ((flags & HANDLE_ACTIVE) != 0) {
      const opt<glm::vec3> hit = on_plane(r, start_position, normal);
      if (hit.has_value()) {
        const glm::vec3 dragged = *hit - start_hit;
        const glm::vec3 moved = start_position + u * snapped(glm::dot(dragged, u), snap) + v * snapped(glm::dot(dragged, v), snap);
        if (moved != position) {
          flags |= HANDLE_CHANGED;
        }
        position = moved;
      }
    }
    return flags;
  }

  uint32_t scene_handles::turn(natural_t id, const glm::vec3& centre, const glm::vec3& axis, float radius, float& angle, float snap, bool near_half_only) {
    PROFILE_SECTION_VERBOSE("scene_handles::turn");
    if (in.cam == nullptr) {
      return HANDLE_NONE;
    }

    /// in reach when the pointer is near the ring as it is shown
    glm::vec3 first;
    glm::vec3 second;
    ring_basis(axis, first, second);
    opt<float> distance = std::nullopt;
    const glm::vec3 to_camera = glm::normalize(in.cam->position - centre);
    glm::vec3 from = centre + first * radius;
    for (int32_t i = 1; i <= kRingSegments; ++i) {
      const float a = kTwoPi * static_cast<float>(i) / static_cast<float>(kRingSegments);
      const glm::vec3 to = centre + (first * std::cos(a) + second * std::sin(a)) * radius;
      /// a ring that faces the camera has no far half: all of it is as near as its centre
      const bool behind = glm::dot((from + to) * 0.5f - centre, to_camera) < -0.01f * radius;
      const opt<std::pair<glm::vec2, glm::vec2>> shown = near_half_only && behind ? std::nullopt : view_segment(*in.cam, in.view_size, from, to);
      if (shown.has_value()) {
        const float d = distance_to_segment(in.pointer, shown->first, shown->second);
        distance = distance.has_value() ? std::min(*distance, d) : d;
      }
      from = to;
    }

    const pick_ray r = view_ray(*in.cam, in.view_size, in.pointer);
    uint32_t flags = track(id, distance);
    if ((flags & HANDLE_STARTED) != 0) {
      last_angle = angle_on_ring(r, centre, axis).value_or(0.f);
      start_hit = first * std::cos(last_angle) + second * std::sin(last_angle);
      turned = 0.f;
      given = 0.f;
      angle = 0.f;
      return flags;
    }
    if ((flags & HANDLE_CANCELLED) != 0) {
      angle = 0.f;
      return flags;
    }
    if ((flags & HANDLE_ACTIVE) != 0) {
      /// each frame's part of a turn is added up, so that it can go on past a half turn.
      ///  a ring seen edge on has no place the pointer is at on it: it is left where it is
      const opt<float> now = angle_on_ring(r, centre, axis);
      if (now.has_value()) {
        float step = *now - last_angle;
        step -= kTwoPi * std::round(step / kTwoPi);
        turned += step;
        last_angle = *now;
      }
      const float out = snapped(turned, snap);
      if (out != given) {
        flags |= HANDLE_CHANGED;
      }
      given = out;
      angle = out;
    }
    return flags;
  }

  uint32_t scene_handles::stretch_axis(natural_t id, const glm::vec3& centre, const glm::vec3& axis, float reach, float& factor, float snap) {
    PROFILE_SECTION_VERBOSE("scene_handles::stretch_axis");
    if (in.cam == nullptr) {
      return HANDLE_NONE;
    }
    const opt<std::pair<glm::vec2, glm::vec2>> shown = view_segment(*in.cam, in.view_size, centre, centre + axis * reach);
    uint32_t flags = track(id, shown.has_value() ? opt<float>{ distance_to_segment(in.pointer, shown->first, shown->second) } : std::nullopt);

    const pick_ray r = view_ray(*in.cam, in.view_size, in.pointer);
    if ((flags & HANDLE_STARTED) != 0) {
      /// taken hold of at the centre itself there is nothing to be so many times as far as
      start_along = std::max(along_axis(r, centre, axis).value_or(reach), reach * 0.05f);
      given = 1.f;
      factor = 1.f;
      return flags;
    }
    if ((flags & HANDLE_CANCELLED) != 0) {
      factor = 1.f;
      return flags;
    }
    if ((flags & HANDLE_ACTIVE) != 0) {
      const opt<float> along = along_axis(r, centre, axis);
      float out = given;
      if (along.has_value()) {
        out = std::max(snapped(*along / start_along, snap), kSmallestFactor);
      }
      if (out != given) {
        flags |= HANDLE_CHANGED;
      }
      given = out;
      factor = out;
    }
    return flags;
  }

  uint32_t scene_handles::stretch_all(natural_t id, const glm::vec3& centre, float radius_pixels, float& factor, float snap) {
    PROFILE_SECTION_VERBOSE("scene_handles::stretch_all");
    if (in.cam == nullptr) {
      return HANDLE_NONE;
    }
    const opt<glm::vec2> shown = view_place(*in.cam, in.view_size, centre);
    opt<float> distance = std::nullopt;
    if (shown.has_value() && glm::distance(in.pointer, *shown) <= radius_pixels) {
      distance = 0.f;
    }

    uint32_t flags = track(id, distance);
    if ((flags & HANDLE_STARTED) != 0) {
      given = 1.f;
      factor = 1.f;
      return flags;
    }
    if ((flags & HANDLE_CANCELLED) != 0) {
      factor = 1.f;
      return flags;
    }
    if ((flags & HANDLE_ACTIVE) != 0) {
      /// to the right and up is bigger: up is a smaller y
      const glm::vec2 dragged = in.pointer - start_pointer;
      const float out = std::max(snapped(1.f + (dragged.x - dragged.y) / kPixelsToDouble, snap), kSmallestFactor);
      if (out != given) {
        flags |= HANDLE_CHANGED;
      }
      given = out;
      factor = out;
    }
    return flags;
  }

}  // namespace other
