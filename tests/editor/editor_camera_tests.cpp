/**
 * \file tests/editor/editor_camera_tests.cpp
 **/
#include <cmath>

#include <gtest/gtest.h>

#include "math/bounding_box.hpp"

#include "renderer/camera.hpp"

#include "other_test.hpp"
#include "services/editor_camera.hpp"

namespace other {
  namespace {

    constexpr float kNear = 1e-3f;

    void expect_near(const glm::vec3& got, const glm::vec3& wanted, float tolerance = kNear) {
      EXPECT_NEAR(got.x, wanted.x, tolerance);
      EXPECT_NEAR(got.y, wanted.y, tolerance);
      EXPECT_NEAR(got.z, wanted.z, tolerance);
    }

  }  // namespace

  class editor_camera_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      cam.position = { 0.f, 1.f, 4.5f };
      cam.look_at({ 0.f, 0.f, 0.f });
      controller.focus_distance = glm::length(cam.position);
    }

    /// to the right as the view shows it
    glm::vec3 view_right() const {
      return glm::normalize(glm::cross(cam.direction, cam.world_up));
    }

    camera cam;
    editor_camera_controller controller;
  };

  TEST_F(editor_camera_tests, flying_covers_the_same_ground_in_a_second_at_any_frame_rate) {
    const glm::vec3 start = cam.position;
    const glm::vec3 forward = cam.direction;

    for (int32_t frame = 0; frame < 30; ++frame) {
      controller.update(cam, { .over_view = true, .look_held = true, .move = { 0.f, 0.f, 1.f }, .seconds = 1.f / 30.f });
    }
    const glm::vec3 at_thirty = cam.position;

    cam.position = start;
    for (int32_t frame = 0; frame < 240; ++frame) {
      controller.update(cam, { .over_view = true, .look_held = true, .move = { 0.f, 0.f, 1.f }, .seconds = 1.f / 240.f });
    }

    expect_near(at_thirty, start + forward * controller.settings.move_speed, 1e-2f);
    expect_near(cam.position, at_thirty, 1e-2f);
  }

  TEST_F(editor_camera_tests, the_keys_fly_to_the_views_right_up_and_faster) {
    const glm::vec3 start = cam.position;
    const glm::vec3 right = view_right();

    controller.update(cam, { .over_view = true, .look_held = true, .move = { 1.f, 0.f, 0.f }, .seconds = 0.5f });
    expect_near(cam.position, start + right * controller.settings.move_speed * 0.5f);

    cam.position = start;
    controller.update(cam, { .over_view = true, .look_held = true, .move = { 0.f, 1.f, 0.f }, .fast = true, .seconds = 0.5f });
    expect_near(cam.position, start + cam.world_up * controller.settings.move_speed * controller.settings.fast_factor * 0.5f);
  }

  TEST_F(editor_camera_tests, the_keys_do_nothing_unless_the_right_button_is_held) {
    const glm::vec3 start = cam.position;
    controller.update(cam, { .over_view = true, .move = { 1.f, 1.f, 1.f }, .seconds = 1.f });
    expect_near(cam.position, start);
  }

  TEST_F(editor_camera_tests, looking_turns_right_and_down_with_the_pointer_and_stays_where_it_is) {
    const glm::vec3 start = cam.position;
    const glm::vec3 before = cam.direction;
    const glm::vec3 right = view_right();

    controller.update(cam, { .over_view = true, .look_held = true, .pointer_delta = { 40.f, 0.f } });
    EXPECT_GT(glm::dot(cam.direction, right), glm::dot(before, right));
    expect_near(cam.position, start);

    const float height = cam.direction.y;
    controller.update(cam, { .over_view = true, .look_held = true, .pointer_delta = { 0.f, 40.f } });
    EXPECT_LT(cam.direction.y, height);
    EXPECT_NEAR(glm::length(cam.direction), 1.f, kNear);
  }

  TEST_F(editor_camera_tests, looking_stops_short_of_straight_up) {
    for (int32_t frame = 0; frame < 100; ++frame) {
      controller.update(cam, { .over_view = true, .look_held = true, .pointer_delta = { 0.f, -500.f } });
    }
    EXPECT_LT(cam.direction.y, 1.f);
    EXPECT_GT(cam.direction.y, 0.99f);
    EXPECT_FALSE(glm::any(glm::isnan(cam.get_view_matrix()[0])));
  }

  TEST_F(editor_camera_tests, a_drag_begins_only_over_the_view_and_goes_on_when_the_pointer_leaves) {
    const glm::vec3 before = cam.direction;
    controller.update(cam, { .over_view = false, .look_held = true, .pointer_delta = { 40.f, 0.f } });
    expect_near(cam.direction, before);
    EXPECT_EQ(controller.current_drag(), editor_camera_controller::drag::NONE);

    /// the button is still held from outside: coming over the view begins it
    controller.update(cam, { .over_view = true, .look_held = true });
    EXPECT_EQ(controller.current_drag(), editor_camera_controller::drag::LOOK);

    controller.update(cam, { .over_view = false, .look_held = true, .pointer_delta = { 40.f, 0.f } });
    EXPECT_EQ(controller.current_drag(), editor_camera_controller::drag::LOOK);
    EXPECT_GT(glm::distance(cam.direction, before), 0.01f);

    controller.update(cam, { .over_view = false });
    EXPECT_EQ(controller.current_drag(), editor_camera_controller::drag::NONE);
  }

  TEST_F(editor_camera_tests, orbiting_keeps_its_distance_from_the_focus_point_and_looks_at_it) {
    const glm::vec3 pivot = controller.focus_point(cam);
    const float distance = controller.focus_distance;
    const glm::vec3 start = cam.position;

    for (int32_t frame = 0; frame < 20; ++frame) {
      controller.update(cam, { .over_view = true, .orbit_held = true, .pointer_delta = { 15.f, -6.f } });
    }

    EXPECT_GT(glm::distance(cam.position, start), 0.5f);
    EXPECT_NEAR(glm::distance(cam.position, pivot), distance, 1e-2f);
    expect_near(glm::normalize(pivot - cam.position), cam.direction, 1e-2f);
    expect_near(controller.focus_point(cam), pivot, 1e-2f);
  }

  TEST_F(editor_camera_tests, panning_slides_the_view_and_looks_the_same_way) {
    const glm::vec3 start = cam.position;
    const glm::vec3 direction = cam.direction;
    const glm::vec3 right = view_right();

    /// the pointer drags the scene with it: to the right, so the camera goes to the left
    controller.update(cam, { .over_view = true, .pan_held = true, .pointer_delta = { 100.f, 0.f }, .view_height = 500.f });

    expect_near(cam.direction, direction);
    const glm::vec3 moved = cam.position - start;
    EXPECT_LT(glm::dot(moved, right), 0.f);
    EXPECT_NEAR(glm::dot(moved, direction), 0.f, kNear);

    /// a fifth of the view's height is a fifth of what the view shows at the focus point
    const float shown = 2.f * controller.focus_distance * std::tan(glm::radians(cam.fov) * 0.5f);
    EXPECT_NEAR(glm::length(moved), shown * 0.2f, 1e-2f);
  }

  TEST_F(editor_camera_tests, the_wheel_goes_towards_the_focus_point_and_never_through_it) {
    const glm::vec3 pivot = controller.focus_point(cam);
    const float distance = controller.focus_distance;

    controller.update(cam, { .over_view = true, .wheel = 1.f });
    EXPECT_LT(glm::distance(cam.position, pivot), distance);
    expect_near(controller.focus_point(cam), pivot, 1e-2f);

    for (int32_t notch = 0; notch < 200; ++notch) {
      controller.update(cam, { .over_view = true, .wheel = 1.f });
    }
    EXPECT_GT(glm::dot(pivot - cam.position, cam.direction), 0.f);

    const float near_distance = controller.focus_distance;
    controller.update(cam, { .over_view = true, .wheel = -1.f });
    EXPECT_GT(controller.focus_distance, near_distance);
  }

  TEST_F(editor_camera_tests, the_wheel_does_nothing_when_the_pointer_is_elsewhere) {
    const glm::vec3 start = cam.position;
    controller.update(cam, { .over_view = false, .wheel = 3.f });
    expect_near(cam.position, start);
  }

  TEST_F(editor_camera_tests, the_wheel_sets_how_fast_it_flies_while_looking) {
    const glm::vec3 start = cam.position;
    controller.update(cam, { .over_view = true, .look_held = true, .wheel = 2.f });
    expect_near(cam.position, start);
    EXPECT_GT(controller.fly_speed_scale, 1.f);

    controller.update(cam, { .over_view = true, .look_held = true, .move = { 0.f, 0.f, 1.f }, .seconds = 1.f });
    EXPECT_GT(glm::distance(cam.position, start), controller.settings.move_speed);
  }

  TEST_F(editor_camera_tests, focusing_puts_a_box_in_the_middle_of_the_view_and_all_of_it_in) {
    const bounding_box box{ { 9.f, 1.f, -4.f }, { 11.f, 3.f, -2.f } };
    const glm::vec3 centre{ 10.f, 2.f, -3.f };
    const glm::vec3 direction = cam.direction;

    controller.focus(cam, box);

    expect_near(cam.direction, direction);
    expect_near(glm::normalize(centre - cam.position), cam.direction);
    expect_near(controller.focus_point(cam), centre);

    /// every corner is in front of the camera and inside the view's angle
    const float half_angle = glm::radians(cam.fov) * 0.5f;
    for (int32_t corner = 0; corner < 8; ++corner) {
      const glm::vec3 point{ (corner & 1) != 0 ? box.max.x : box.min.x, (corner & 2) != 0 ? box.max.y : box.min.y, (corner & 4) != 0 ? box.max.z : box.min.z };
      const glm::vec3 to_point = glm::normalize(point - cam.position);
      EXPECT_LT(std::acos(glm::dot(to_point, cam.direction)), half_angle);
    }
  }

}  // namespace other
