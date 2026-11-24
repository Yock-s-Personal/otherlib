/**
 * \file services/scene_pick.cpp
 **/
#include "services/scene_pick.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "core/profiler.hpp"
#include "data-structures/std_container.hpp"
#include "math/bounding_box.hpp"

#include "model/animation_limits.hpp"
#include "model/model.hpp"
#include "model/model_source.hpp"
#include "renderer/camera.hpp"

#include "object/render_component.hpp"
#include "scene/scene.hpp"

namespace other {
  namespace {

    constexpr float kParallel = 1e-8f;

    /// what the renderer draws the view with, made here so that the camera is only read
    glm::mat4 view_projection(const camera& cam, const glm::vec2& size) {
      const glm::mat4 view = glm::lookAt(cam.position, cam.position + cam.direction, cam.up());
      const glm::mat4 projection = glm::perspective(glm::radians(cam.fov), size.x / size.y, cam.clip.near_plane, cam.clip.far_plane);
      return projection * view;
    }

    /// Moller-Trumbore, with the direction as it is given and not made length one: the
    ///  answer is then in lengths of that direction
    opt<float> ray_triangle_distance(const glm::vec3& origin, const glm::vec3& direction, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
      const glm::vec3 ab = b - a;
      const glm::vec3 ac = c - a;
      const glm::vec3 p = glm::cross(direction, ac);
      const float det = glm::dot(ab, p);
      if (std::abs(det) < kParallel) {
        return std::nullopt;
      }

      const float inv_det = 1.f / det;
      const glm::vec3 from_a = origin - a;
      const float u = glm::dot(from_a, p) * inv_det;
      if (u < 0.f || u > 1.f) {
        return std::nullopt;
      }

      const glm::vec3 q = glm::cross(from_a, ab);
      const float v = glm::dot(direction, q) * inv_det;
      if (v < 0.f || u + v > 1.f) {
        return std::nullopt;
      }

      const float t = glm::dot(ac, q) * inv_det;
      if (t <= 0.f) {
        return std::nullopt;
      }
      return t;
    }

    opt<float> ray_box_distance(const glm::vec3& origin, const glm::vec3& direction, const bounding_box& box) {
      float enter = 0.f;
      float leave = std::numeric_limits<float>::max();
      for (int32_t axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < kParallel) {
          if (origin[axis] < box.min[axis] || origin[axis] > box.max[axis]) {
            return std::nullopt;
          }
          continue;
        }

        float near_side = (box.min[axis] - origin[axis]) / direction[axis];
        float far_side = (box.max[axis] - origin[axis]) / direction[axis];
        if (near_side > far_side) {
          std::swap(near_side, far_side);
        }
        enter = std::max(enter, near_side);
        leave = std::min(leave, far_side);
        if (enter > leave) {
          return std::nullopt;
        }
      }
      return enter;
    }

    /// a vertex as the vertex shader places it under a pose (get_bone_transform): the
    ///  weighted sum of its bones, and where it was modelled when no bone has a say
    glm::vec3 posed_position(const vertex& vert, std::span<const glm::mat4> palette) {
      glm::mat4 bone{ 0.f };
      bool influenced = false;
      const int32_t bones = static_cast<int32_t>(std::min(palette.size(), kMaxBones));
      for (int32_t i = 0; i < 4; ++i) {
        const int32_t id = vert.bone_ids[i];
        if (id < 0 || id >= bones) {
          continue;
        }
        bone += palette[id] * vert.bone_weights[i];
        influenced = true;
      }
      return influenced ? glm::vec3(bone * glm::vec4(vert.position, 1.f)) : vert.position;
    }

    /// where a posed model's vertices can be: each joint's box of what it moves, moved by it
    bounding_box posed_bounds(const model_data& data, std::span<const glm::mat4> palette) {
      bounding_box posed = bounding_box::empty;
      const size_t joints = std::min(palette.size(), data.skel.joints.size());
      for (size_t i = 0; i < joints; ++i) {
        bounding_box moved = data.skel.joints[i].influenced_bounds;
        if (moved == bounding_box::empty) {
          continue;
        }
        posed = bounding_box::expand_to_include(posed, moved.transform(palette[i]));
      }
      return posed;
    }

  }  // namespace

  pick_ray view_ray(const camera& cam, const glm::vec2& size, const glm::vec2& place) {
    if (size.x <= 0.f || size.y <= 0.f) {
      return { .origin = cam.position, .direction = glm::normalize(cam.direction) };
    }

    const glm::vec2 ndc{ 2.f * place.x / size.x - 1.f, 1.f - 2.f * place.y / size.y };
    const glm::mat4 to_world = glm::inverse(view_projection(cam, size));
    const glm::vec4 near_point = to_world * glm::vec4(ndc, -1.f, 1.f);
    const glm::vec4 far_point = to_world * glm::vec4(ndc, 1.f, 1.f);
    const glm::vec3 from = glm::vec3(near_point) / near_point.w;
    const glm::vec3 to = glm::vec3(far_point) / far_point.w;
    return { .origin = cam.position, .direction = glm::normalize(to - from) };
  }

  opt<glm::vec2> view_place(const camera& cam, const glm::vec2& size, const glm::vec3& point) {
    const glm::vec4 clip = view_projection(cam, size) * glm::vec4(point, 1.f);
    if (clip.w <= 0.f) {
      return std::nullopt;
    }
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    return glm::vec2{ (ndc.x + 1.f) * 0.5f * size.x, (1.f - ndc.y) * 0.5f * size.y };
  }

  float view_units_per_pixel(const camera& cam, const glm::vec2& size, const glm::vec3& point) {
    const float depth = glm::dot(point - cam.position, glm::normalize(cam.direction));
    return 2.f * std::abs(depth) * std::tan(glm::radians(cam.fov) * 0.5f) / std::max(size.y, 1.f);
  }

  opt<float> ray_box_distance(const pick_ray& r, const bounding_box& box) {
    return ray_box_distance(r.origin, r.direction, box);
  }

  opt<float> ray_model_distance(const pick_ray& r, const model& m, const glm::mat4& world) {
    PROFILE_SECTION("ray_model_distance");
    if (m.source == nullptr) {
      return std::nullopt;
    }
    const model_data& data = m.source->source_data();

    /// the ray is taken to where the model was made, and the triangles are left where they
    ///  are. its direction is not made length one again, so a distance along it there is
    ///  the same distance along the ray in the world
    const glm::mat4 to_model = glm::inverse(world);
    const glm::vec3 origin = glm::vec3(to_model * glm::vec4(r.origin, 1.f));
    const glm::vec3 direction = glm::vec3(to_model * glm::vec4(r.direction, 0.f));
    if (glm::any(glm::isnan(origin)) || glm::any(glm::isnan(direction))) {
      return std::nullopt;
    }

    const std::span<const glm::mat4> palette{ m.bone_matrices };
    opt<bounding_box> posed_box = std::nullopt;
    ostd::vector<glm::vec3> posed;
    opt<float> nearest = std::nullopt;

    for (const uint32_t sm_idx : m.submesh_indices) {
      if (sm_idx >= data.submeshes.size()) {
        continue;
      }
      const submesh& sm = data.submeshes[sm_idx];
      /// the same test the scene makes when it decides a draw is skinned
      const bool skinned = sm.rigged && !palette.empty();

      if (skinned) {
        if (!posed_box.has_value()) {
          posed_box = posed_bounds(data, palette);
        }
        if (!(*posed_box == bounding_box::empty) && !ray_box_distance(origin, direction, *posed_box).has_value()) {
          continue;
        }
        posed.resize(sm.vert_cnt);
        for (uint32_t v = 0; v < sm.vert_cnt; ++v) {
          posed[v] = posed_position(data.vertices[sm.base_vertex + v], palette);
        }
      } else if (!(sm.bounds == bounding_box::empty) && !ray_box_distance(origin, direction, sm.bounds).has_value()) {
        continue;
      }

      /// a submesh's indices count from its own first vertex, three to a triangle
      const size_t first = sm.base_idx / 3;
      const size_t count = sm.idx_cnt / 3;
      for (size_t i = first; i < first + count && i < data.indices.size(); ++i) {
        const index& tri = data.indices[i];
        if (tri.v0 >= sm.vert_cnt || tri.v1 >= sm.vert_cnt || tri.v2 >= sm.vert_cnt) {
          continue;
        }
        const glm::vec3& a = skinned ? posed[tri.v0] : data.vertices[sm.base_vertex + tri.v0].position;
        const glm::vec3& b = skinned ? posed[tri.v1] : data.vertices[sm.base_vertex + tri.v1].position;
        const glm::vec3& c = skinned ? posed[tri.v2] : data.vertices[sm.base_vertex + tri.v2].position;
        const opt<float> distance = ray_triangle_distance(origin, direction, a, b, c);
        if (distance.has_value() && (!nearest.has_value() || *distance < *nearest)) {
          nearest = distance;
        }
      }
    }
    return nearest;
  }

  opt<pick_hit> pick_object(scene& s, const pick_ray& r) {
    PROFILE_SECTION("pick_object");
    opt<pick_hit> nearest = std::nullopt;
    for (const uint64_t id : s.get_all_object_ids()) {
      if (!s.is_visible(id)) {
        continue;
      }
      /// source stays null until the model's asset has loaded
      const render_component* render = s.try_get_component<render_component>(id);
      if (render == nullptr || !render->visible || render->obj_model.source == nullptr) {
        continue;
      }

      const opt<float> distance = ray_model_distance(r, render->obj_model, s.get_world_transform(id));
      if (distance.has_value() && (!nearest.has_value() || *distance < nearest->distance)) {
        nearest = pick_hit{ .object = id, .distance = *distance };
      }
    }
    return nearest;
  }

}  // namespace other
