/**
 * \file math/euler.hpp
 *
 * the one order the engine reads Euler angles in, so that the Inspector and a script mean
 * the same turn by the same three numbers
 **/
#ifndef OTHER_CORE_MATH_EULER_HPP
#define OTHER_CORE_MATH_EULER_HPP

#include "math/definitions.hpp"

namespace other {

  /// degrees round x (pitch), round y (yaw) and round z (roll). the roll is made first,
  ///  then the pitch, then the yaw: so yaw turns round the world's up whatever the pitch
  ///  and runs the whole circle, and pitch is the one that stops at 90 either way
  glm::quat quat_from_euler(const glm::vec3& degrees);

  /// the angles quat_from_euler makes this rotation of: pitch from -90 to 90, yaw and roll
  ///  from -180 to 180. looking straight up or down, yaw and roll are one turn and it is
  ///  all given as yaw
  glm::vec3 euler_from_quat(const glm::quat& rotation);

}  // namespace other

#endif  // OTHER_CORE_MATH_EULER_HPP
