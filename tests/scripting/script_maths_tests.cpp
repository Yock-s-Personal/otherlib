/**
 * \file tests/scripting/script_maths_tests.cpp
 *
 * the maths a C# script is given (Mathf, Vec2, Vec3, Vec4, Quaternion) gives the answers
 * the probe has worked out by hand
 **/
#include "math/euler.hpp"

#include "script/scripting_environment.hpp"

#include "other_test.hpp"

namespace other {

  class script_maths_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    template <typename... Args>
    int32_t probe(const std::string_view method, Args... args) {
      auto* env = subsystem<scripting_environment>::get();
      return env->call_static_dotnet_method<int32_t>("MathsProbe", method, args...);
    }

    /// every check of one of the probe's groups. only a number comes back, so a check
    ///  that fails says which it was on the error stream
    void expect_group_passes(const int32_t group, const std::string_view name) {
      const int32_t count = probe("Count", group);
      ASSERT_GT(count, 0) << name << " has no checks";
      for (int32_t index = 0; index < count; ++index) {
        EXPECT_EQ(probe("Passes", group, index), 1) << name << " check " << index;
      }
    }
  };

  TEST_F(script_maths_tests, mathf_gives_the_answers_worked_out_by_hand) {
    expect_group_passes(0, "Mathf");
  }

  TEST_F(script_maths_tests, vec2_gives_the_answers_worked_out_by_hand) {
    expect_group_passes(1, "Vec2");
  }

  TEST_F(script_maths_tests, vec3_gives_the_answers_worked_out_by_hand) {
    expect_group_passes(2, "Vec3");
  }

  TEST_F(script_maths_tests, vec4_gives_the_answers_worked_out_by_hand) {
    expect_group_passes(3, "Vec4");
  }

  TEST_F(script_maths_tests, quaternion_gives_the_answers_worked_out_by_hand) {
    expect_group_passes(4, "Quaternion");
  }

  TEST_F(script_maths_tests, a_script_and_the_engine_mean_the_same_by_euler_angles) {
    /// the Inspector shows the engine's reading and a script sets its own: the same three
    ///  numbers have to be the same turn in both, and read back the same
    struct whole_degrees {
      int32_t pitch;
      int32_t yaw;
      int32_t roll;
    };
    for (const whole_degrees& a : { whole_degrees{ 0, 90, 0 }, whole_degrees{ 90, 0, 0 }, whole_degrees{ 0, 0, 90 }, whole_degrees{ 30, 40, 50 }, whole_degrees{ -75, 160, -110 }, whole_degrees{ 10, -135, 80 }, whole_degrees{ 90, 30, 50 } }) {
      const glm::quat made = quat_from_euler(glm::vec3{ static_cast<float>(a.pitch), static_cast<float>(a.yaw), static_cast<float>(a.roll) });
      EXPECT_NEAR(probe("EulerPart", a.pitch, a.yaw, a.roll, 0) / 1e6f, made.x, 3e-6f) << a.pitch << " " << a.yaw << " " << a.roll;
      EXPECT_NEAR(probe("EulerPart", a.pitch, a.yaw, a.roll, 1) / 1e6f, made.y, 3e-6f) << a.pitch << " " << a.yaw << " " << a.roll;
      EXPECT_NEAR(probe("EulerPart", a.pitch, a.yaw, a.roll, 2) / 1e6f, made.z, 3e-6f) << a.pitch << " " << a.yaw << " " << a.roll;
      EXPECT_NEAR(probe("EulerPart", a.pitch, a.yaw, a.roll, 3) / 1e6f, made.w, 3e-6f) << a.pitch << " " << a.yaw << " " << a.roll;

      const glm::vec3 read = euler_from_quat(made);
      /// straight up, the pitch is read through a sine that is flat there: near enough
      const float near_enough = a.pitch == 90 ? 0.1f : 3e-3f;
      EXPECT_NEAR(probe("EulerPart", a.pitch, a.yaw, a.roll, 4) / 1e3f, read.x, near_enough) << a.pitch << " " << a.yaw << " " << a.roll;
      EXPECT_NEAR(probe("EulerPart", a.pitch, a.yaw, a.roll, 5) / 1e3f, read.y, near_enough) << a.pitch << " " << a.yaw << " " << a.roll;
      EXPECT_NEAR(probe("EulerPart", a.pitch, a.yaw, a.roll, 6) / 1e3f, read.z, near_enough) << a.pitch << " " << a.yaw << " " << a.roll;
    }
  }

  TEST_F(script_maths_tests, the_probe_has_no_group_the_suite_does_not_run) {
    EXPECT_EQ(probe("Count", 5), -1);
  }

}  // namespace other
