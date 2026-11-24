/**
 * \file tests/editor/scene_handles_tests.cpp
 *
 * the handles of the Scene View, told what the pointer did frame by frame as its window
 * tells them, with no window
 **/
#include <cmath>
#include <numbers>

#include <gtest/gtest.h>

#include "renderer/camera.hpp"

#include "other_test.hpp"
#include "services/scene_handles.hpp"
#include "services/scene_pick.hpp"

namespace other {
  namespace {

    constexpr float kNear = 1e-2f;
    constexpr natural_t kX = 11;
    constexpr natural_t kY = 12;
    constexpr natural_t kPlane = 13;
    constexpr natural_t kRing = 14;
    constexpr natural_t kMiddle = 15;

    const glm::vec3 kRight{ 1.f, 0.f, 0.f };
    const glm::vec3 kUp{ 0.f, 1.f, 0.f };
    const glm::vec3 kTowards{ 0.f, 0.f, 1.f };

    bool has(uint32_t flags, handle_flags flag) {
      return (flags & flag) != 0;
    }

  }  // namespace

  class scene_handles_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      /// ten in front of the origin, looking at it: x is to the right and y is up
      cam.position = { 0.f, 0.f, 10.f };
      cam.look_at({ 0.f, 0.f, 0.f });
    }

    /// one frame with the pointer over a point of the world
    handle_input frame_at(const glm::vec3& point, bool down = false, bool pressed = false) const {
      return {
        .cam = &cam,
        .view_size = size,
        .pointer = view_place(cam, size, point).value_or(glm::vec2{ -100.f, -100.f }),
        .over_view = true,
        .down = down,
        .pressed = pressed,
      };
    }

    /// the pointer comes to rest on a point, so that what is there is hot, and presses.
    ///  each frame the handles are asked for by `ask`, which gives back its flags
    template <typename Ask>
    uint32_t press_at(const glm::vec3& point, Ask&& ask) {
      handles.begin_frame(frame_at(point));
      ask();
      handles.end_frame();
      handles.begin_frame(frame_at(point, true, true));
      const uint32_t flags = ask();
      handles.end_frame();
      return flags;
    }

    template <typename Ask>
    uint32_t drag_to(const glm::vec3& point, Ask&& ask) {
      handles.begin_frame(frame_at(point, true));
      const uint32_t flags = ask();
      handles.end_frame();
      return flags;
    }

    template <typename Ask>
    uint32_t let_go_at(const glm::vec3& point, Ask&& ask) {
      handles.begin_frame(frame_at(point));
      const uint32_t flags = ask();
      handles.end_frame();
      return flags;
    }

    camera cam;
    glm::vec2 size{ 800.f, 600.f };
    scene_handles handles;
  };

  TEST_F(scene_handles_tests, a_handle_is_hot_under_the_pointer_and_dragged_along_its_axis_only) {
    glm::vec3 position{ 0.f, 0.f, 0.f };
    const auto ask = [&]() -> uint32_t {
      return handles.slide_axis(kX, position, kRight, 1.f);
    };

    /// the frame the pointer arrives it is only seen to be nearest; the next it is hot
    handles.begin_frame(frame_at({ 0.5f, 0.f, 0.f }));
    EXPECT_FALSE(has(ask(), HANDLE_HOT));
    handles.end_frame();
    handles.begin_frame(frame_at({ 0.5f, 0.f, 0.f }));
    EXPECT_TRUE(has(ask(), HANDLE_HOT));
    EXPECT_TRUE(handles.any_hot());
    handles.end_frame();

    handles.begin_frame(frame_at({ 0.5f, 0.f, 0.f }, true, true));
    uint32_t flags = ask();
    handles.end_frame();
    EXPECT_TRUE(has(flags, HANDLE_STARTED));
    EXPECT_TRUE(has(flags, HANDLE_ACTIVE));
    EXPECT_TRUE(handles.any_active());
    EXPECT_TRUE(handles.press_taken());
    EXPECT_NEAR(position.x, 0.f, kNear);

    /// the pointer goes two to the right and a little up: the handle goes two to the right
    flags = drag_to({ 2.5f, 0.2f, 0.f }, ask);
    EXPECT_TRUE(has(flags, HANDLE_ACTIVE));
    EXPECT_TRUE(has(flags, HANDLE_CHANGED));
    EXPECT_NEAR(position.x, 2.f, kNear);
    EXPECT_NEAR(position.y, 0.f, kNear);
    EXPECT_NEAR(position.z, 0.f, kNear);

    /// held still it has not changed
    flags = drag_to({ 2.5f, 0.2f, 0.f }, ask);
    EXPECT_TRUE(has(flags, HANDLE_ACTIVE));
    EXPECT_FALSE(has(flags, HANDLE_CHANGED));

    flags = let_go_at({ 2.5f, 0.2f, 0.f }, ask);
    EXPECT_TRUE(has(flags, HANDLE_ENDED));
    EXPECT_FALSE(has(flags, HANDLE_CANCELLED));
    EXPECT_FALSE(handles.any_active());
    EXPECT_NEAR(position.x, 2.f, kNear);
  }

  TEST_F(scene_handles_tests, a_drag_goes_in_steps_when_it_snaps) {
    glm::vec3 position{ 0.f, 0.f, 0.f };
    const auto ask = [&]() -> uint32_t {
      return handles.slide_axis(kX, position, kRight, 1.f, 0.5f);
    };
    ASSERT_TRUE(has(press_at({ 0.5f, 0.f, 0.f }, ask), HANDLE_STARTED));

    drag_to({ 2.1f, 0.f, 0.f }, ask);
    EXPECT_NEAR(position.x, 1.5f, 1e-4f);
    drag_to({ 2.4f, 0.f, 0.f }, ask);
    EXPECT_NEAR(position.x, 2.f, 1e-4f);
  }

  TEST_F(scene_handles_tests, escape_puts_back_what_was_dragged_and_says_so) {
    glm::vec3 position{ 1.f, 2.f, 3.f };
    const auto ask = [&]() -> uint32_t {
      return handles.slide_axis(kX, position, kRight, 1.f);
    };
    ASSERT_TRUE(has(press_at({ 1.5f, 2.f, 3.f }, ask), HANDLE_STARTED));
    drag_to({ 3.f, 2.f, 3.f }, ask);
    ASSERT_GT(position.x, 2.f);

    handle_input cancelled = frame_at({ 3.f, 2.f, 3.f }, true);
    cancelled.cancel = true;
    handles.begin_frame(cancelled);
    const uint32_t flags = ask();
    handles.end_frame();
    EXPECT_TRUE(has(flags, HANDLE_CANCELLED));
    EXPECT_TRUE(has(flags, HANDLE_ENDED));
    EXPECT_EQ(position, (glm::vec3{ 1.f, 2.f, 3.f }));

    /// the button is still held: nothing is dragged by it any more
    EXPECT_EQ(drag_to({ 4.f, 2.f, 3.f }, ask) & (HANDLE_ACTIVE | HANDLE_STARTED), 0u);
    EXPECT_EQ(position, (glm::vec3{ 1.f, 2.f, 3.f }));
  }

  TEST_F(scene_handles_tests, a_press_away_from_every_handle_or_with_alt_takes_none) {
    glm::vec3 position{ 0.f, 0.f, 0.f };
    const auto ask = [&]() -> uint32_t {
      return handles.slide_axis(kX, position, kRight, 1.f);
    };

    EXPECT_EQ(press_at({ 0.f, 3.f, 0.f }, ask), 0u);
    EXPECT_FALSE(handles.any_active());
    EXPECT_FALSE(handles.press_taken());
    let_go_at({ 0.f, 3.f, 0.f }, ask);

    /// on the handle, with alt held: the press is the camera's
    handles.begin_frame(frame_at({ 0.5f, 0.f, 0.f }));
    ask();
    handles.end_frame();
    handle_input with_alt = frame_at({ 0.5f, 0.f, 0.f }, true, true);
    with_alt.blocked = true;
    handles.begin_frame(with_alt);
    const uint32_t flags = ask();
    handles.end_frame();
    EXPECT_TRUE(has(flags, HANDLE_HOT));
    EXPECT_FALSE(has(flags, HANDLE_ACTIVE));
    EXPECT_FALSE(handles.press_taken());
  }

  TEST_F(scene_handles_tests, of_two_handles_the_one_nearer_the_pointer_is_hot) {
    glm::vec3 position{ 0.f, 0.f, 0.f };
    uint32_t x = 0;
    uint32_t y = 0;
    const auto ask = [&]() -> uint32_t {
      x = handles.slide_axis(kX, position, kRight, 1.f);
      y = handles.slide_axis(kY, position, kUp, 1.f);
      return x;
    };

    /// close to where they meet, a little nearer the one that goes up
    const glm::vec3 nearer_y{ 0.02f, 0.06f, 0.f };
    handles.begin_frame(frame_at(nearer_y));
    ask();
    handles.end_frame();
    handles.begin_frame(frame_at(nearer_y));
    ask();
    handles.end_frame();
    EXPECT_FALSE(has(x, HANDLE_HOT));
    EXPECT_TRUE(has(y, HANDLE_HOT));
  }

  TEST_F(scene_handles_tests, a_square_handle_is_dragged_in_its_plane) {
    glm::vec3 position{ 0.f, 0.f, 0.f };
    const auto ask = [&]() -> uint32_t {
      return handles.slide_plane(kPlane, position, kRight, kUp, 0.2f, 0.6f);
    };

    /// outside the square nothing is taken
    EXPECT_EQ(press_at({ 0.9f, 0.9f, 0.f }, ask), 0u);
    let_go_at({ 0.9f, 0.9f, 0.f }, ask);

    ASSERT_TRUE(has(press_at({ 0.4f, 0.4f, 0.f }, ask), HANDLE_STARTED));
    const uint32_t flags = drag_to({ 1.4f, -0.6f, 0.f }, ask);
    EXPECT_TRUE(has(flags, HANDLE_CHANGED));
    EXPECT_NEAR(position.x, 1.f, kNear);
    EXPECT_NEAR(position.y, -1.f, kNear);
    EXPECT_NEAR(position.z, 0.f, kNear);
  }

  TEST_F(scene_handles_tests, a_ring_gives_the_angle_it_has_been_taken_round_and_goes_on_past_half_a_turn) {
    const glm::vec3 centre{ 0.f, 0.f, 0.f };
    float angle = 7.f;
    const auto ask = [&]() -> uint32_t {
      return handles.turn(kRing, centre, kTowards, 1.f, angle);
    };

    /// asked for and not dragged, the angle is left alone
    handles.begin_frame(frame_at({ 1.f, 0.f, 0.f }));
    ask();
    handles.end_frame();
    EXPECT_FLOAT_EQ(angle, 7.f);

    ASSERT_TRUE(has(press_at({ 1.f, 0.f, 0.f }, ask), HANDLE_STARTED));
    EXPECT_FLOAT_EQ(angle, 0.f);

    /// from the right of the ring to its top: a quarter turn counter-clockwise, seen from
    ///  where the axis points, which is where the camera is
    const float quarter = std::numbers::pi_v<float> / 2.f;
    uint32_t flags = drag_to({ 0.f, 1.f, 0.f }, ask);
    EXPECT_TRUE(has(flags, HANDLE_CHANGED));
    EXPECT_NEAR(angle, quarter, kNear);

    drag_to({ -1.f, 0.f, 0.f }, ask);
    EXPECT_NEAR(angle, 2.f * quarter, kNear);
    drag_to({ 0.f, -1.f, 0.f }, ask);
    EXPECT_NEAR(angle, 3.f * quarter, kNear);

    /// back the way it came
    drag_to({ -1.f, 0.f, 0.f }, ask);
    drag_to({ 0.f, 1.f, 0.f }, ask);
    drag_to({ 1.f, 0.f, 0.f }, ask);
    drag_to({ 0.f, -1.f, 0.f }, ask);
    EXPECT_NEAR(angle, -quarter, kNear);
  }

  TEST_F(scene_handles_tests, a_ring_says_where_it_was_taken_hold_of) {
    float angle = 0.f;
    const auto ask = [&]() -> uint32_t {
      return handles.turn(kRing, { 0.f, 0.f, 0.f }, kTowards, 1.f, angle);
    };
    ASSERT_TRUE(has(press_at({ 0.f, 1.f, 0.f }, ask), HANDLE_STARTED));
    EXPECT_LT(glm::distance(handles.grab_direction(), kUp), kNear);

    /// turned on, it is still where it was taken hold of that is said
    drag_to({ -1.f, 0.f, 0.f }, ask);
    EXPECT_LT(glm::distance(handles.grab_direction(), kUp), kNear);
  }

  TEST_F(scene_handles_tests, a_ring_round_a_ball_is_taken_hold_of_by_its_near_half_only) {
    /// from above and in front, a ring that lies flat is seen as an oval: its far side is
    ///  higher in the picture than its near side
    cam.position = { 0.f, 5.f, 10.f };
    cam.look_at({ 0.f, 0.f, 0.f });
    float angle = 0.f;
    const glm::vec3 far_side{ 0.f, 0.f, -2.f };
    const glm::vec3 near_side{ 0.f, 0.f, 2.f };

    const auto whole = [&]() -> uint32_t {
      return handles.turn(kRing, { 0.f, 0.f, 0.f }, kUp, 2.f, angle);
    };
    EXPECT_TRUE(has(press_at(far_side, whole), HANDLE_STARTED));
    let_go_at(far_side, whole);

    const auto near_half = [&]() -> uint32_t {
      return handles.turn(kRing, { 0.f, 0.f, 0.f }, kUp, 2.f, angle, 0.f, true);
    };
    EXPECT_EQ(press_at(far_side, near_half), 0u);
    let_go_at(far_side, near_half);
    EXPECT_TRUE(has(press_at(near_side, near_half), HANDLE_STARTED));
    let_go_at(near_side, near_half);

    /// a ring that faces the camera has no far half: it is taken hold of anywhere
    const glm::vec3 to_camera = glm::normalize(cam.position);
    const glm::vec3 across = glm::normalize(glm::cross(kUp, to_camera));
    const auto facing = [&]() -> uint32_t {
      return handles.turn(kRing, { 0.f, 0.f, 0.f }, to_camera, 2.f, angle, 0.f, true);
    };
    EXPECT_TRUE(has(press_at(across * 2.f, facing), HANDLE_STARTED));
    let_go_at(across * 2.f, facing);
    EXPECT_TRUE(has(press_at(across * -2.f, facing), HANDLE_STARTED));
  }

  TEST_F(scene_handles_tests, a_ring_snaps_to_its_step) {
    float angle = 0.f;
    const float step = std::numbers::pi_v<float> / 12.f;
    const auto ask = [&]() -> uint32_t {
      return handles.turn(kRing, { 0.f, 0.f, 0.f }, kTowards, 1.f, angle, step);
    };
    ASSERT_TRUE(has(press_at({ 1.f, 0.f, 0.f }, ask), HANDLE_STARTED));

    /// forty degrees round is nearer three steps of fifteen than two
    const float forty = 40.f * std::numbers::pi_v<float> / 180.f;
    drag_to({ std::cos(forty), std::sin(forty), 0.f }, ask);
    EXPECT_NEAR(angle, 3.f * step, 1e-4f);
  }

  TEST_F(scene_handles_tests, stretching_gives_how_many_times_as_far_the_pointer_is) {
    float factor = 5.f;
    const auto ask = [&]() -> uint32_t {
      return handles.stretch_axis(kX, { 0.f, 0.f, 0.f }, kRight, 1.f, factor);
    };
    ASSERT_TRUE(has(press_at({ 0.8f, 0.f, 0.f }, ask), HANDLE_STARTED));
    EXPECT_FLOAT_EQ(factor, 1.f);

    drag_to({ 1.6f, 0.3f, 0.f }, ask);
    EXPECT_NEAR(factor, 2.f, kNear);
    drag_to({ 0.4f, 0.f, 0.f }, ask);
    EXPECT_NEAR(factor, 0.5f, kNear);
    /// through the centre and out the other side is not a size: it stops at very small
    drag_to({ -1.f, 0.f, 0.f }, ask);
    EXPECT_GT(factor, 0.f);
    EXPECT_LT(factor, 0.05f);
  }

  TEST_F(scene_handles_tests, the_round_handle_grows_to_the_right_and_up) {
    float factor = 1.f;
    const auto ask = [&]() -> uint32_t {
      return handles.stretch_all(kMiddle, { 0.f, 0.f, 0.f }, 10.f, factor);
    };
    ASSERT_TRUE(has(press_at({ 0.f, 0.f, 0.f }, ask), HANDLE_STARTED));

    handle_input in = frame_at({ 0.f, 0.f, 0.f }, true);
    in.pointer += glm::vec2{ 100.f, 0.f };
    handles.begin_frame(in);
    ask();
    handles.end_frame();
    EXPECT_NEAR(factor, 2.f, kNear);

    in.pointer = frame_at({ 0.f, 0.f, 0.f }).pointer + glm::vec2{ 0.f, 50.f };
    handles.begin_frame(in);
    ask();
    handles.end_frame();
    EXPECT_NEAR(factor, 0.5f, kNear);
  }

  TEST_F(scene_handles_tests, a_handle_nobody_asks_for_any_more_is_let_go) {
    glm::vec3 position{ 0.f, 0.f, 0.f };
    const auto ask = [&]() -> uint32_t {
      return handles.slide_axis(kX, position, kRight, 1.f);
    };
    ASSERT_TRUE(has(press_at({ 0.5f, 0.f, 0.f }, ask), HANDLE_STARTED));

    /// its tool is gone: a frame goes by in which it is not asked for
    handles.begin_frame(frame_at({ 1.f, 0.f, 0.f }, true));
    handles.end_frame();
    EXPECT_FALSE(handles.any_active());
  }

  TEST_F(scene_handles_tests, an_axis_that_points_at_the_camera_moves_nothing_and_breaks_nothing) {
    /// seen end on it is a dot, and a ray along it has no nearest place on it
    glm::vec3 position{ 0.f, 0.f, 0.f };
    const auto ask = [&]() -> uint32_t {
      return handles.slide_axis(kX, position, kTowards, 1.f);
    };
    press_at({ 0.f, 0.f, 0.f }, ask);
    drag_to({ 0.f, 0.f, 0.f }, ask);
    EXPECT_FALSE(glm::any(glm::isnan(position)));
    EXPECT_NEAR(glm::length(position), 0.f, kNear);
  }

  TEST_F(scene_handles_tests, a_line_that_goes_behind_the_camera_is_cut_where_it_does) {
    const opt<std::pair<glm::vec2, glm::vec2>> in_front = view_segment(cam, size, { -1.f, 0.f, 0.f }, { 1.f, 0.f, 0.f });
    ASSERT_TRUE(in_front.has_value());
    EXPECT_LT(in_front->first.x, 400.f);
    EXPECT_GT(in_front->second.x, 400.f);

    /// from the origin to twenty behind where the camera stands: what is shown stops short
    const opt<std::pair<glm::vec2, glm::vec2>> cut = view_segment(cam, size, { 1.f, 0.f, 0.f }, { 1.f, 0.f, 30.f });
    ASSERT_TRUE(cut.has_value());
    EXPECT_FALSE(glm::any(glm::isnan(cut->second)));
    EXPECT_GT(cut->second.x, cut->first.x);

    EXPECT_FALSE(view_segment(cam, size, { 0.f, 0.f, 20.f }, { 1.f, 0.f, 30.f }).has_value());
  }

}  // namespace other
