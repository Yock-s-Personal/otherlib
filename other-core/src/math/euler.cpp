/**
 * \file math/euler.cpp
 **/
#include "math/euler.hpp"

#include <cmath>

namespace other {

  glm::quat quat_from_euler(const glm::vec3& degrees) {
    const glm::vec3 half = glm::radians(degrees) * 0.5f;
    const float sp = std::sin(half.x);
    const float cp = std::cos(half.x);
    const float sy = std::sin(half.y);
    const float cy = std::cos(half.y);
    const float sr = std::sin(half.z);
    const float cr = std::cos(half.z);

    /// the product yaw * pitch * roll, written out
    const float x = cy * sp * cr + sy * cp * sr;
    const float y = sy * cp * cr - cy * sp * sr;
    const float z = cy * cp * sr - sy * sp * cr;
    const float w = cy * cp * cr + sy * sp * sr;
    return glm::quat{ w, x, y, z };
  }

  glm::vec3 euler_from_quat(const glm::quat& rotation) {
    /// a quaternion of all zeroes is how a scene writes no rotation
    const float length = glm::length(rotation);
    if (length <= 0.f) {
      return glm::vec3{ 0.f, 0.f, 0.f };
    }

    /// read off where the rotation sends the three axes
    const glm::quat unit = rotation / length;
    const glm::vec3 right = unit * glm::vec3{ 1.f, 0.f, 0.f };
    const glm::vec3 up = unit * glm::vec3{ 0.f, 1.f, 0.f };
    const glm::vec3 forward = unit * glm::vec3{ 0.f, 0.f, 1.f };

    const float pitch = std::asin(glm::clamp(-forward.y, -1.f, 1.f));
    if (std::abs(forward.y) < 0.999999f) {
      return glm::degrees(glm::vec3{ pitch, std::atan2(forward.x, forward.z), std::atan2(right.y, up.y) });
    }
    /// straight up or down: forward has no heading left to read the yaw from
    return glm::degrees(glm::vec3{ pitch, std::atan2(-right.z, right.x), 0.f });
  }

}  // namespace other
