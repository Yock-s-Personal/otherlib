/**
 * \file tests/editor/scene_pick_tests.cpp
 *
 * what a place in the editor's view is in the world, and the object a ray from it meets.
 * the models are made here by hand: a model keeps its triangles on the CPU, so none of
 * this needs a window
 **/
#include <array>
#include <cmath>

#include <glm/ext/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "math/bounding_box.hpp"

#include "model/model_data.hpp"
#include "model/model_source.hpp"
#include "renderer/camera.hpp"

#include "object/render_component.hpp"
#include "scene/scene.hpp"

#include "other_test.hpp"
#include "services/editor_bindings.hpp"
#include "services/editor_services.hpp"
#include "services/scene_pick.hpp"

namespace other {
  namespace {

    constexpr float kNear = 1e-3f;

    /// adds a flat square of two triangles to the model as a submesh of its own, facing
    ///  +z, its middle at the offset and its sides two long
    void add_square(model_data& data, const glm::vec3& offset, bool rigged = false) {
      /// a model is not one without a node for its submeshes to hang from
      if (data.nodes.empty()) {
        mesh_node& node = data.nodes.emplace_back();
        node.name = "root";
        node.local_transform = glm::mat4(1.f);
      }
      data.nodes[0].sub_meshes.push_back(static_cast<uint32_t>(data.submeshes.size()));

      submesh& sm = data.submeshes.emplace_back();
      sm.base_vertex = static_cast<uint32_t>(data.vertices.size());
      sm.base_idx = static_cast<uint32_t>(data.indices.size() * 3);
      sm.vert_cnt = 4;
      sm.idx_cnt = 6;
      sm.local_transform = glm::mat4(1.f);
      sm.rigged = rigged;
      sm.bounds = bounding_box{ offset + glm::vec3{ -1.f, -1.f, 0.f }, offset + glm::vec3{ 1.f, 1.f, 0.f } };

      const glm::vec3 corners[4] = { { -1.f, -1.f, 0.f }, { 1.f, -1.f, 0.f }, { 1.f, 1.f, 0.f }, { -1.f, 1.f, 0.f } };
      for (const glm::vec3& corner : corners) {
        vertex& vert = data.vertices.emplace_back();
        vert.position = offset + corner;
        if (rigged) {
          vert.bone_weights = { 1.f, 0.f, 0.f, 0.f };
        }
      }
      /// counted from the submesh's own first vertex, as an imported model's are
      data.indices.push_back({ 0, 1, 2 });
      data.indices.push_back({ 0, 2, 3 });
      data.bounds = bounding_box::expand_to_include(data.bounds, sm.bounds);
    }

    model_data square_model(const glm::vec3& offset = { 0.f, 0.f, 0.f }) {
      model_data data;
      data.name = "square";
      add_square(data, offset);
      return data;
    }

    /// straight at the plane z = 0 from five in front of it
    pick_ray ray_at(float x, float y) {
      return { .origin = { x, y, 5.f }, .direction = { 0.f, 0.f, -1.f } };
    }

  }  // namespace

  class scene_pick_tests : public other_test {
   protected:
    /// a scene makes itself a physics world
    bool script_and_physics() const override { return true; }

    natural_t place(scene& s, const std::string& name, model_source& source, const glm::vec3& at) {
      scene_object& object = s.create_object(name, at);
      render_component render = {};
      render.obj_model = source.produce_model();
      s.add_component<render_component>(&object, std::move(render));
      return object.id;
    }
  };

  TEST_F(scene_pick_tests, a_ray_through_a_place_of_the_view_passes_through_what_is_shown_there) {
    camera cam;
    cam.position = { 0.f, 1.f, 4.5f };
    cam.look_at({ 0.f, 0.f, 0.f });
    const glm::vec2 size{ 800.f, 600.f };

    const glm::vec3 point{ 1.f, 0.5f, -2.f };
    const opt<glm::vec2> shown = view_place(cam, size, point);
    ASSERT_TRUE(shown.has_value());
    EXPECT_GT(shown->x, 400.f);
    EXPECT_LT(shown->x, 800.f);

    const pick_ray r = view_ray(cam, size, *shown);
    const glm::vec3 to_point = point - r.origin;
    const glm::vec3 nearest = r.origin + r.direction * glm::dot(to_point, r.direction);
    EXPECT_LT(glm::distance(nearest, point), kNear);
    EXPECT_NEAR(glm::length(r.direction), 1.f, kNear);

    /// the middle of the picture is where the camera looks
    const pick_ray middle = view_ray(cam, size, size * 0.5f);
    EXPECT_LT(glm::distance(middle.direction, glm::normalize(cam.direction)), kNear);
    const opt<glm::vec2> looked_at = view_place(cam, size, { 0.f, 0.f, 0.f });
    ASSERT_TRUE(looked_at.has_value());
    EXPECT_NEAR(looked_at->x, 400.f, 0.1f);
    EXPECT_NEAR(looked_at->y, 300.f, 0.1f);

    /// up in the world is up in the picture, which is a smaller y
    const opt<glm::vec2> above = view_place(cam, size, { 0.f, 1.f, 0.f });
    ASSERT_TRUE(above.has_value());
    EXPECT_LT(above->y, 300.f);

    EXPECT_FALSE(view_place(cam, size, { 0.f, 1.f, 9.f }).has_value());
  }

  TEST_F(scene_pick_tests, a_pixel_covers_as_much_of_the_world_as_the_view_says_it_does) {
    camera cam;
    cam.position = { 0.f, 0.f, 10.f };
    cam.look_at({ 0.f, 0.f, 0.f });
    const glm::vec2 size{ 800.f, 600.f };

    const float units = view_units_per_pixel(cam, size, { 0.f, 0.f, 0.f });
    const opt<glm::vec2> a = view_place(cam, size, { 0.f, 0.f, 0.f });
    const opt<glm::vec2> b = view_place(cam, size, { 1.f, 0.f, 0.f });
    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_NEAR((b->x - a->x) * units, 1.f, 1e-2f);

    /// twice as far is twice as much
    EXPECT_NEAR(view_units_per_pixel(cam, size, { 0.f, 0.f, -10.f }), units * 2.f, 1e-3f);
  }

  TEST_F(scene_pick_tests, a_ray_says_how_far_it_is_to_a_box_and_when_it_misses) {
    const bounding_box box{ { -1.f, -1.f, -1.f }, { 1.f, 1.f, 1.f } };

    const opt<float> hit = ray_box_distance(ray_at(0.5f, 0.5f), box);
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(*hit, 4.f, kNear);

    EXPECT_FALSE(ray_box_distance(ray_at(1.5f, 0.f), box).has_value());
    /// pointing away from it
    EXPECT_FALSE(ray_box_distance({ .origin = { 0.f, 0.f, 5.f }, .direction = { 0.f, 0.f, 1.f } }, box).has_value());
    /// from inside it
    const opt<float> inside = ray_box_distance({ .origin = { 0.f, 0.f, 0.f }, .direction = { 1.f, 0.f, 0.f } }, box);
    ASSERT_TRUE(inside.has_value());
    EXPECT_NEAR(*inside, 0.f, kNear);
    /// past a corner: inside two of its three pairs of sides and never inside all three
    const glm::vec3 past{ 0.f, 0.f, -1.f };
    EXPECT_FALSE(ray_box_distance({ .origin = { 1.2f, 1.2f, 5.f }, .direction = past }, box).has_value());
  }

  TEST_F(scene_pick_tests, the_nearer_of_two_objects_is_met_and_a_hidden_one_is_not) {
    model_source source{ square_model() };
    scene s{ "Pick Scene" };
    const natural_t front = place(s, "Front", source, { 0.f, 0.f, 0.f });
    const natural_t back = place(s, "Back", source, { 0.f, 0.f, -3.f });

    opt<pick_hit> hit = pick_object(s, ray_at(0.2f, 0.3f));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->object, front);
    EXPECT_NEAR(hit->distance, 5.f, kNear);

    s.set_object_visible(front, false);
    hit = pick_object(s, ray_at(0.2f, 0.3f));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->object, back);
    EXPECT_NEAR(hit->distance, 8.f, kNear);

    /// drawn or not is the render component's say as well
    s.try_get_component<render_component>(back)->visible = false;
    EXPECT_FALSE(pick_object(s, ray_at(0.2f, 0.3f)).has_value());

    EXPECT_FALSE(pick_object(s, ray_at(4.f, 0.f)).has_value());
  }

  TEST_F(scene_pick_tests, a_ray_inside_an_objects_box_that_misses_its_triangles_meets_what_is_behind) {
    /// one triangle: half of the square its box is
    model_data half = square_model();
    half.indices.pop_back();
    half.submeshes[0].idx_cnt = 3;
    model_source triangle{ std::move(half) };
    model_source square{ square_model() };

    scene s{ "Pick Scene" };
    const natural_t front = place(s, "Half", triangle, { 0.f, 0.f, 0.f });
    const natural_t back = place(s, "Whole", square, { 0.f, 0.f, -3.f });

    /// the triangle is the corner by 1, -1: this is on it
    opt<pick_hit> hit = pick_object(s, ray_at(0.5f, -0.5f));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->object, front);

    /// and this is in its box but off it
    hit = pick_object(s, ray_at(-0.5f, 0.5f));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->object, back);
  }

  TEST_F(scene_pick_tests, an_object_is_met_where_it_stands_at_the_size_it_is_and_from_behind) {
    model_source source{ square_model() };
    scene s{ "Pick Scene" };
    const natural_t moved = place(s, "Moved", source, { 6.f, 0.f, 0.f });
    s.get_transform(moved).local_scale = glm::vec3(2.f);

    EXPECT_FALSE(pick_object(s, ray_at(0.f, 0.f)).has_value());
    /// 1.8 from its middle: off the square as modelled and on it at twice the size
    const opt<pick_hit> hit = pick_object(s, ray_at(7.8f, 0.f));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->object, moved);
    EXPECT_NEAR(hit->distance, 5.f, kNear);
    EXPECT_FALSE(pick_object(s, ray_at(8.2f, 0.f)).has_value());

    /// either side of a triangle counts
    const opt<pick_hit> behind = pick_object(s, { .origin = { 6.f, 0.f, -5.f }, .direction = { 0.f, 0.f, 1.f } });
    ASSERT_TRUE(behind.has_value());
    EXPECT_EQ(behind->object, moved);
  }

  TEST_F(scene_pick_tests, each_submesh_is_met_through_its_own_vertices) {
    /// the second square's indices count from its own first vertex, not the model's
    model_data data = square_model();
    add_square(data, { 4.f, 0.f, 0.f });
    model_source source{ std::move(data) };
    scene s{ "Pick Scene" };
    const natural_t both = place(s, "Both", source, { 0.f, 0.f, 0.f });

    opt<pick_hit> hit = pick_object(s, ray_at(4.5f, 0.5f));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->object, both);
    EXPECT_FALSE(pick_object(s, ray_at(2.f, 0.f)).has_value());

    /// a submesh whose box nobody set is met all the same: its triangles are what count
    model_data unboxed = square_model();
    unboxed.submeshes[0].bounds = bounding_box{};
    model_source no_box{ std::move(unboxed) };
    const natural_t plain = place(s, "No Box", no_box, { 0.f, 5.f, 0.f });
    hit = pick_object(s, ray_at(0.5f, 5.5f));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->object, plain);

    /// a model shown with only its first submesh is not met at its second
    s.try_get_component<render_component>(both)->obj_model = source.produce_model("first only", std::array<uint32_t, 1>{ 0u });
    EXPECT_FALSE(pick_object(s, ray_at(4.5f, 0.5f)).has_value());
    EXPECT_TRUE(pick_object(s, ray_at(0.5f, 0.5f)).has_value());
  }

  TEST_F(scene_pick_tests, a_posed_mesh_is_met_where_its_pose_puts_it) {
    model_data data;
    data.name = "rigged square";
    add_square(data, { 0.f, 0.f, 0.f }, true);
    joint& bone = data.skel.joints.emplace_back();
    bone.name = "bone";
    bone.influenced_bounds = data.submeshes[0].bounds;
    model_source source{ std::move(data) };

    scene s{ "Pick Scene" };
    const natural_t posed = place(s, "Posed", source, { 0.f, 0.f, 0.f });
    model& shown = s.try_get_component<render_component>(posed)->obj_model;

    /// with no pose it is where it was modelled
    EXPECT_TRUE(pick_object(s, ray_at(0.f, 0.f)).has_value());

    /// its one bone carries it five to the right: its box, were it asked, would still be
    ///  round where it was modelled
    shown.bone_matrices = { glm::translate(glm::mat4(1.f), glm::vec3{ 5.f, 0.f, 0.f }) };
    EXPECT_FALSE(pick_object(s, ray_at(0.f, 0.f)).has_value());
    const opt<pick_hit> hit = pick_object(s, ray_at(5.5f, 0.5f));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->object, posed);
    EXPECT_NEAR(hit->distance, 5.f, kNear);
  }

  TEST_F(scene_pick_tests, the_view_answers_for_places_of_the_screen_as_its_window_drew_it) {
    model_source source{ square_model() };
    scene s{ "Pick Scene" };
    const natural_t square = place(s, "Square", source, { 0.f, 0.f, 0.f });

    camera cam;
    cam.position = { 0.f, 0.f, 5.f };
    cam.look_at({ 0.f, 0.f, 0.f });

    editor_services services;
    services.current_selection.scene_ptr = &s;
    bindings::set_editor_services(&services);

    /// no view yet: nothing is anywhere
    float rectangle[4] = { 9.f, 9.f, 9.f, 9.f };
    float place_on_screen[2] = { 0.f, 0.f };
    float origin[3] = {};
    float direction[3] = {};
    EXPECT_EQ(bindings::native_editor_view_state(rectangle), 0u);
    EXPECT_EQ(bindings::native_editor_view_pick(500.f, 350.f), 0u);
    EXPECT_FALSE(bindings::native_editor_view_place(0.f, 0.f, 0.f, place_on_screen));
    EXPECT_FALSE(bindings::native_editor_view_ray(500.f, 350.f, origin, direction));

    /// the view's picture is 800 by 600 with its top left at 100, 50 on the screen
    services.view_camera = &cam;
    services.view = { .shown = true, .position = { 100.f, 50.f }, .size = { 800.f, 600.f }, .hovered = true, .clicked = false };
    EXPECT_EQ(bindings::native_editor_view_state(rectangle), 3u);
    EXPECT_FLOAT_EQ(rectangle[0], 100.f);
    EXPECT_FLOAT_EQ(rectangle[3], 600.f);

    /// what the camera looks at is in the middle of the picture, wherever that is on the screen
    ASSERT_TRUE(bindings::native_editor_view_place(0.f, 0.f, 0.f, place_on_screen));
    EXPECT_NEAR(place_on_screen[0], 500.f, 0.1f);
    EXPECT_NEAR(place_on_screen[1], 350.f, 0.1f);
    EXPECT_EQ(bindings::native_editor_view_pick(500.f, 350.f), square);
    EXPECT_EQ(services.pick_in_view({ 500.f, 350.f }), square);

    /// a corner of the square, and a place just past it
    ASSERT_TRUE(bindings::native_editor_view_place(0.95f, 0.95f, 0.f, place_on_screen));
    EXPECT_EQ(bindings::native_editor_view_pick(place_on_screen[0], place_on_screen[1]), square);
    ASSERT_TRUE(bindings::native_editor_view_place(1.1f, 1.1f, 0.f, place_on_screen));
    EXPECT_EQ(bindings::native_editor_view_pick(place_on_screen[0], place_on_screen[1]), 0u);

    ASSERT_TRUE(bindings::native_editor_view_ray(500.f, 350.f, origin, direction));
    EXPECT_NEAR(origin[2], 5.f, kNear);
    EXPECT_NEAR(direction[2], -1.f, kNear);
    EXPECT_NEAR(bindings::native_editor_view_pixel_size(0.f, 0.f, 0.f), 2.f * 5.f / 600.f, kNear);

    bindings::set_editor_services(nullptr);
    EXPECT_EQ(bindings::native_editor_view_pick(500.f, 350.f), 0u);
  }

}  // namespace other
