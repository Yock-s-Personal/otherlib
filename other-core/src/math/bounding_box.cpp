/**
 * \file math/bounding_box.cpp
 **/
#include "math/bounding_box.hpp"

#include "math/constants.hpp"

namespace other {

  bounding_box bounding_box::empty = bounding_box(glm::vec3(infinity), glm::vec3(-infinity));
  bounding_box bounding_box::infinite = bounding_box(glm::vec3(-infinity), glm::vec3(infinity));

  bool bounding_box::contains(const glm::vec3& point) const {
    return (point.x >= min.x && point.x <= max.x) &&
      (point.y >= min.y && point.y <= max.y) &&
      (point.z >= min.z && point.z <= max.z);
  }

  bool bounding_box::intersects(const bounding_box& other) const {
    return (min.x <= other.max.x && max.x >= other.min.x) &&
      (min.y <= other.max.y && max.y >= other.min.y) &&
      (min.z <= other.max.z && max.z >= other.min.z);
  }

  bool bounding_box::intersects(const ray& r) const {
    /// the stretch of the ray that is between every pair of sides at once: it starts where
    ///  the ray does and is cut down by each pair in turn. none left is a miss
    real_t enter = 0;
    real_t leave = infinity;
    for (uint32_t i = 0; i < 3; ++i) {
      if (std::abs(r.direction[i]) < epsilon) {
        if (r.origin[i] < min[i] || r.origin[i] > max[i]) {
          return false;
        }
        continue;
      }

      const real_t inv_dir = 1.0f / r.direction[i];
      real_t t0 = (min[i] - r.origin[i]) * inv_dir;
      real_t t1 = (max[i] - r.origin[i]) * inv_dir;
      if (t0 > t1) {
        std::swap(t0, t1);
      }

      enter = std::max(enter, t0);
      leave = std::min(leave, t1);
      if (enter > leave) {
        return false;
      }
    }

    return true;
  }

  bounding_box bounding_box::transform(const glm::mat4& t) {
    /// all eight corners - mapping min/max alone breaks under any rotation
    bounding_box box;
    for (int i = 0; i < 8; ++i) {
      const glm::vec3 corner{ (i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 4) ? max.z : min.z };
      const glm::vec3 p = glm::vec3(t * glm::vec4(corner, 1.f));
      box.min = glm::min(box.min, p);
      box.max = glm::max(box.max, p);
    }
    return box;
  }

  bounding_box bounding_box::expand_to_include(const bounding_box& box, const bounding_box& other) {
    bounding_box result = box;
    result.min = glm::min(result.min, other.min);
    result.max = glm::max(result.max, other.max);
    return result;
  }

}  // namespace other
