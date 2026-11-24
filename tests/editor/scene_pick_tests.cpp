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

#include "object/camera_component.hpp"
#include "object/light_component.hpp"
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

  TEST_F(scene_pick_tests, a_light_and_a_camera_have_an_icon_that_is_clicked_before_what_is_behind_it) {
    model_source source{ square_model() };
    scene s{ "Pick Scene" };
    const natural_t square = place(s, "Square", source, { 0.f, 0.f, 0.f });
    scene_object& lamp = s.create_object("Lamp", glm::vec3{ 0.5f, 0.5f, 2.f });
    s.add_component<point_light_component>(&lamp).light.position = { 0.f, 0.f, 0.f };
    scene_object& filmed = s.create_object("Filmed", glm::vec3{ 9.f, 9.f, 9.f });
    /// a camera is where it says it is, which is not where its object is
    s.add_component<camera_component>(&filmed).camera.position = { -0.5f, 0.5f, 1.f };
    scene_object& behind = s.create_object("Behind", glm::vec3{ 0.f, 0.f, 20.f });
    s.add_component<point_light_component>(&behind).light.position = { 0.f, 0.f, 0.f };

    camera cam;
    cam.position = { 0.f, 0.f, 5.f };
    cam.look_at({ 0.f, 0.f, 0.f });

    editor_services services;
    services.current_selection.scene_ptr = &s;
    services.view_camera = &cam;
    services.view = { .shown = true, .position = { 100.f, 50.f }, .size = { 800.f, 600.f } };
    services.set_selection(lamp.id);

    /// one for the lamp and one for the camera; the light behind the view's camera has none
    ostd::vector<editor_services::view_icon> icons;
    services.collect_view_icons(icons);
    ASSERT_EQ(icons.size(), 2u);
    const auto of = [&](natural_t object) -> const editor_services::view_icon& {
      return *std::ranges::find(icons, object, &editor_services::view_icon::object);
    };
    EXPECT_EQ(of(lamp.id).what, editor_services::view_icon::POINT_LIGHT);
    EXPECT_TRUE(of(lamp.id).selected);
    EXPECT_EQ(of(filmed.id).what, editor_services::view_icon::CAMERA);
    EXPECT_FALSE(of(filmed.id).selected);
    const opt<glm::vec2> lamp_shown = view_place(cam, services.view.size, { 0.5f, 0.5f, 2.f });
    ASSERT_TRUE(lamp_shown.has_value());
    EXPECT_NEAR(of(lamp.id).place.x, lamp_shown->x, kNear);
    EXPECT_NEAR(of(lamp.id).place.y, lamp_shown->y, kNear);

    /// both icons are in front of the square: on an icon it is the icon's object, a little
    ///  off it and past its reach it is the square
    const glm::vec2 on_lamp = services.view.position + of(lamp.id).place;
    EXPECT_EQ(services.pick_in_view(on_lamp), lamp.id);
    EXPECT_EQ(services.pick_in_view(on_lamp + glm::vec2{ 5.f, 5.f }), lamp.id);
    EXPECT_EQ(services.pick_in_view(on_lamp + glm::vec2{ 0.f, editor_services::kIconReach + 3.f }), square);
    EXPECT_EQ(services.pick_in_view(services.view.position + of(filmed.id).place), filmed.id);

    /// an object that is hidden has no icon
    s.set_object_visible(lamp.id, false);
    services.collect_view_icons(icons);
    EXPECT_EQ(icons.size(), 1u);
    EXPECT_EQ(services.pick_in_view(on_lamp), square);
  }

  TEST_F(scene_pick_tests, a_model_is_placed_where_the_pointer_is_over_the_scene_as_one_undo_step) {
    model_source source{ square_model() };
    scene s{ "Pick Scene" };
    place(s, "Square", source, { 0.f, 0.f, 0.f });

    camera cam;
    cam.position = { 0.f, 2.f, 5.f };
    cam.look_at({ 0.f, 0.f, 0.f });

    editor_services services;
    services.current_selection.scene_ptr = &s;
    services.view_camera = &cam;
    services.view = { .shown = true, .position = { 0.f, 0.f }, .size = { 800.f, 600.f } };
    services.tick_edit_tracker();

    /// with nothing to load a model with, nothing is placed
    EXPECT_EQ(services.place_model("models/crate.fbx", { 400.f, 300.f }), 0u);

    std::string asked_for;
    services.begin_asset_load = [&asked_for](const std::string_view path) -> natural_t {
      asked_for = path;
      return path.ends_with(".fbx") ? 77u : 0u;
    };
    /// one that cannot be loaded is not placed either
    EXPECT_EQ(services.place_model("models/crate.txt", { 400.f, 300.f }), 0u);
    EXPECT_FALSE(services.can_undo_scene_edit());

    /// the middle of the view is the square's middle: the model goes on it
    const size_t before = s.get_all_object_ids().size();
    const natural_t on_square = services.place_model("models/crate.fbx", { 400.f, 300.f });
    ASSERT_NE(on_square, 0u);
    EXPECT_EQ(asked_for, "models/crate.fbx");
    EXPECT_EQ(s.get_object(on_square).name, "crate");
    EXPECT_LT(glm::length(glm::vec3(s.get_world_transform(on_square)[3])), 1e-2f);
    ASSERT_NE(s.try_get_component<render_component>(on_square), nullptr);
    EXPECT_EQ(s.try_get_component<render_component>(on_square)->model_asset_id, 77u);
    EXPECT_EQ(services.active_selection(), on_square);
    EXPECT_EQ(s.get_all_object_ids().size(), before + 1);

    /// below the square the pointer is over the ground and nothing else: it goes there
    const opt<glm::vec2> ground = view_place(cam, services.view.size, { 3.f, 0.f, 2.f });
    ASSERT_TRUE(ground.has_value());
    const natural_t on_ground = services.place_model("models/crate.fbx", *ground);
    ASSERT_NE(on_ground, 0u);
    const glm::vec3 placed = glm::vec3(s.get_world_transform(on_ground)[3]);
    EXPECT_NEAR(placed.x, 3.f, 1e-2f);
    EXPECT_NEAR(placed.y, 0.f, 1e-2f);
    EXPECT_NEAR(placed.z, 2.f, 1e-2f);

    /// at the sky there is nothing to put it on: it is put a little in front of the camera
    const natural_t in_the_air = services.place_model("models/crate.fbx", { 400.f, 5.f });
    ASSERT_NE(in_the_air, 0u);
    EXPECT_NEAR(glm::distance(glm::vec3(s.get_world_transform(in_the_air)[3]), cam.position), 5.f, 1e-2f);

    /// each was one step: three undos and there is only the square again
    for (int32_t i = 0; i < 3; ++i) {
      ASSERT_TRUE(services.can_undo_scene_edit());
      services.undo_scene_edit();
    }
    EXPECT_FALSE(services.can_undo_scene_edit());
    EXPECT_EQ(s.get_all_object_ids().size(), before);
  }

}  // namespace other
