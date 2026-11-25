/**
 * \file tests/core/euler_tests.cpp
 *
 * the engine's Euler angles: each turns round its own axis, yaw goes the whole way round,
 * and the three numbers come back as they went in
 **/
#include <gtest/gtest.h>

#include "math/euler.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    const glm::vec3 kX{ 1.f, 0.f, 0.f };
    const glm::vec3 kY{ 0.f, 1.f, 0.f };
    const glm::vec3 kZ{ 0.f, 0.f, 1.f };

    void expect_near(const glm::vec3& got, const glm::vec3& wanted, const float tolerance = 1e-4f) {
      EXPECT_NEAR(got.x, wanted.x, tolerance);
      EXPECT_NEAR(got.y, wanted.y, tolerance);
      EXPECT_NEAR(got.z, wanted.z, tolerance);
    }

    /// two quaternions that turn everything the same way, whichever sign each has
    void expect_same_rotation(const glm::quat& got, const glm::quat& wanted) {
      EXPECT_NEAR(std::abs(glm::dot(got, wanted)), 1.f, 1e-5f);
    }

    glm::quat about(const glm::vec3& axis, const float degrees) {
      return glm::angleAxis(glm::radians(degrees), axis);
    }

  }  // namespace

  class euler_tests : public other_test {};

  TEST_F(euler_tests, each_angle_turns_round_its_own_axis) {
    /// counter-clockwise seen from where the axis points
    expect_near(quat_from_euler({ 90.f, 0.f, 0.f }) * kY, kZ);
    expect_near(quat_from_euler({ 0.f, 90.f, 0.f }) * kZ, kX);
    expect_near(quat_from_euler({ 0.f, 0.f, 90.f }) * kX, kY);
    expect_near(quat_from_euler({ 0.f, 0.f, 0.f }) * glm::vec3{ 1.f, 2.f, 3.f }, { 1.f, 2.f, 3.f });
  }

  TEST_F(euler_tests, the_roll_is_made_first_then_the_pitch_then_the_yaw) {
    expect_same_rotation(quat_from_euler({ 30.f, 40.f, 50.f }), about(kY, 40.f) * about(kX, 30.f) * about(kZ, 50.f));
    /// so a yaw turns what has been pitched round the world's up
    expect_near(quat_from_euler({ -45.f, 90.f, 0.f }) * kZ, glm::normalize(glm::vec3{ 1.f, 1.f, 0.f }));
  }

  TEST_F(euler_tests, the_angles_come_back_as_they_went_in) {
    for (const float pitch : { -80.f, -30.f, 0.f, 45.f, 85.f }) {
      for (const float yaw : { -170.f, -95.f, -10.f, 0.f, 60.f, 90.f, 135.f, 179.f }) {
        for (const float roll : { -120.f, 0.f, 15.f, 170.f }) {
          const glm::vec3 angles{ pitch, yaw, roll };
          expect_near(euler_from_quat(quat_from_euler(angles)), angles, 2e-3f);
          expect_near(euler_from_quat(-quat_from_euler(angles)), angles, 2e-3f);
        }
      }
    }
  }

  TEST_F(euler_tests, yaw_can_be_added_to_for_ever) {
    /// what a script does that spins a thing by adding to its yaw each frame. in an order
    ///  where yaw is the angle that stops at 90 this sticks there
    glm::quat rotation = quat_from_euler({ 20.f, 0.f, 0.f });
    for (int32_t frame = 0; frame < 400; ++frame) {
      glm::vec3 angles = euler_from_quat(rotation);
      angles.y += 3.f;
      rotation = quat_from_euler(angles);
    }
    /// 1200 degrees is three turns and 120 over
    expect_same_rotation(rotation, quat_from_euler({ 20.f, 120.f, 0.f }));
  }

  TEST_F(euler_tests, straight_up_and_down_keep_the_rotation) {
    for (const glm::vec3& angles : { glm::vec3{ 90.f, 30.f, 0.f }, glm::vec3{ -90.f, -140.f, 0.f }, glm::vec3{ 90.f, 30.f, 50.f }, glm::vec3{ -90.f, 10.f, -70.f } }) {
      const glm::quat rotation = quat_from_euler(angles);
      const glm::vec3 read = euler_from_quat(rotation);
      EXPECT_NEAR(read.x, angles.x, 0.1f);
      EXPECT_FLOAT_EQ(read.z, 0.f);
      EXPECT_NEAR(std::abs(glm::dot(quat_from_euler(read), rotation)), 1.f, 1e-4f);
    }
  }

  TEST_F(euler_tests, a_quaternion_of_all_zeroes_is_no_rotation) {
    expect_near(euler_from_quat(glm::quat{ 0.f, 0.f, 0.f, 0.f }), { 0.f, 0.f, 0.f });
    /// and one that is not of length one is read as the rotation it stands for
    expect_near(euler_from_quat(quat_from_euler({ 10.f, 20.f, 30.f }) * 4.f), { 10.f, 20.f, 30.f }, 2e-3f);
  }

}  // namespace other
