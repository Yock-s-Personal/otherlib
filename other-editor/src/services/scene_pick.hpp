/**
 * \file services/scene_pick.hpp
 * what a place in the editor's view of the scene is in the world, and the object a ray from
 * it meets. nothing here asks a window or the GPU for anything: a model keeps its triangles
 * on the CPU, and those are what a ray is tested against
 **/
#ifndef OTHER_EDITOR_SERVICES_SCENE_PICK_HPP
#define OTHER_EDITOR_SERVICES_SCENE_PICK_HPP

#include <glm/glm.hpp>

#include "core/defines.hpp"

namespace other {

  class camera;
  class scene;
  struct bounding_box;
  struct model;

  struct pick_ray {
    glm::vec3 origin = { 0.f, 0.f, 0.f };
    /// of length one, so that a distance along the ray is a distance in the world
    glm::vec3 direction = { 0.f, 0.f, -1.f };
  };

  struct pick_hit {
    natural_t object = 0;
    /// how far along the ray it is met
    float distance = 0.f;
  };

  /// the ray from the camera through a place of its picture. the place is from the
  ///  picture's top left, to the right and down, in the units the size is in
  pick_ray view_ray(const camera& cam, const glm::vec2& size, const glm::vec2& place);

  /// where a point of the world is in the camera's picture, in those units. none when the
  ///  point is behind the camera
  opt<glm::vec2> view_place(const camera& cam, const glm::vec2& size, const glm::vec3& point);

  /// how much of the world one unit of the picture covers at a point's distance: what a
  ///  shape is sized by to stay the same size on the screen
  float view_units_per_pixel(const camera& cam, const glm::vec2& size, const glm::vec3& point);

  /// how far along the ray it enters the box, 0 from inside it. none when it misses
  opt<float> ray_box_distance(const pick_ray& r, const bounding_box& box);

  /// how far along the ray the nearest triangle of the model is, placed in the world by the
  ///  matrix and posed as the model is. none when the ray meets none. either side of a
  ///  triangle counts
  opt<float> ray_model_distance(const pick_ray& r, const model& m, const glm::mat4& world);

  /// the nearest object the ray meets the triangles of, among those that are drawn: one
  ///  that is hidden, or has no model, is not met
  opt<pick_hit> pick_object(scene& s, const pick_ray& r);

}  // namespace other

#endif  // OTHER_EDITOR_SERVICES_SCENE_PICK_HPP
