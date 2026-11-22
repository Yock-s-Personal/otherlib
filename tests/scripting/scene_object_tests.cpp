/**
 * \file tests/scripting/scene_object_tests.cpp
 *
 * a scene object as a script holds it: found, made, tagged, moved under another, placed in
 * the world, and known to be gone when it is
 **/
#include <glm/gtc/quaternion.hpp>

#include "script/scripting_environment.hpp"

#include "object/script_component.hpp"
#include "object/transform.hpp"
#include "scene/scene.hpp"

#include "scripting/dotnet_bindings/component_bindings.hpp"
#include "scripting/dotnet_bindings/scene_bindings.hpp"

#include "other_test.hpp"

namespace other {

  class scene_object_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    /// a scene the script bindings act on for as long as it lives: a hub with a door on it
    ///  and a spoke, both at the top
    struct bound_scene {
      scene s{ "Object Scene" };
      natural_t hub = 0;
      natural_t spoke = 0;

      bound_scene() {
        detail::set_scene_for_bindings(&s);
        scene_object& hub_object = s.create_object("Hub", glm::vec3(10.f, 0.f, 0.f));
        scene_object& spoke_object = s.create_object("Spoke", glm::vec3(1.f, 2.f, 3.f));
        s.get_component<script_component>(&hub_object)->add_behavior("DoorBehavior");
        hub = hub_object.id;
        spoke = spoke_object.id;
      }

      ~bound_scene() {
        detail::set_scene_for_bindings(nullptr);
      }

      bound_scene(const bound_scene&) = delete;
      bound_scene& operator=(const bound_scene&) = delete;
    };

    void SetUp() override {
      other_test::SetUp();
      env = subsystem<scripting_environment>::get();
      ASSERT_NE(env, nullptr);
    }

    int32_t probe(const std::string_view method) {
      return env->call_static_dotnet_method<int32_t>("ObjectProbe", method);
    }

    scripting_environment* env = nullptr;
  };

  TEST_F(scene_object_tests, an_object_is_made_and_found_as_itself) {
    bound_scene bound;
    const size_t before = bound.s.get_object_count();
    EXPECT_EQ(probe("CreateAndFind"), 1);
    EXPECT_EQ(bound.s.get_object_count(), before + 2);
    EXPECT_NE(bound.s.find_object(std::string_view{ "ProbeRenamed" }), nullptr);

    EXPECT_EQ(probe("CreateUnderAParent"), 1);
    const scene_object* child = bound.s.find_object(std::string_view{ "ProbeChild" });
    ASSERT_NE(child, nullptr);
    ASSERT_NE(bound.s.get_parent(child->id), nullptr);
    EXPECT_EQ(bound.s.get_parent(child->id)->id, bound.hub);
  }

  TEST_F(scene_object_tests, objects_are_found_by_tag) {
    bound_scene bound;
    EXPECT_EQ(probe("Tags"), 1);
    EXPECT_FALSE(bound.s.object_has_tag(bound.hub, "probe-tag"));
    EXPECT_TRUE(bound.s.object_has_tag(bound.spoke, "probe-tag"));
  }

  TEST_F(scene_object_tests, a_parent_is_set_with_the_object_left_where_it_is) {
    bound_scene bound;
    EXPECT_EQ(probe("SetParentKeepingTheWorld"), 1);
    /// the probe put it back at the top
    const scene_object* parent = bound.s.get_parent(bound.spoke);
    EXPECT_TRUE(parent == nullptr || parent == &bound.s.root_object());
  }

  TEST_F(scene_object_tests, a_parent_is_set_with_the_object_left_as_it_was_under_it) {
    bound_scene bound;
    EXPECT_EQ(probe("SetParentKeepingItsPlaceUnderIt"), 1);
    ASSERT_NE(bound.s.get_parent(bound.spoke), nullptr);
    EXPECT_EQ(bound.s.get_parent(bound.spoke)->id, bound.hub);
    EXPECT_EQ(bound.s.get_transform(bound.spoke).local_position, glm::vec3(1.f, 2.f, 3.f));
  }

  TEST_F(scene_object_tests, a_place_in_the_world_is_set_and_read_back) {
    bound_scene bound;
    EXPECT_EQ(probe("WorldPosition"), 1);
    EXPECT_EQ(glm::vec3(bound.s.get_world_transform(bound.spoke)[3]), glm::vec3(15.f, 6.f, 7.f));
  }

  TEST_F(scene_object_tests, a_turn_and_a_size_are_read_through_what_an_object_is_under) {
    bound_scene bound;
    transform& hub = bound.s.get_transform(bound.hub);
    hub.local_rotation_quat = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.f, 1.f, 0.f));
    hub.local_scale = glm::vec3(2.f);
    EXPECT_EQ(probe("WorldRotationAndScale"), 1);
  }

  TEST_F(scene_object_tests, an_id_that_is_no_object_is_answered_and_not_followed) {
    bound_scene bound;
    constexpr natural_t kNobody = 4000;
    float three[3] = { 9.f, 9.f, 9.f };
    float four[4] = { 9.f, 9.f, 9.f, 9.f };

    bindings::native_transform_get_world_position(kNobody, three);
    EXPECT_EQ(three[0], 0.f);
    bindings::native_transform_get_world_scale(kNobody, three);
    EXPECT_EQ(three[2], 1.f);
    bindings::native_transform_get_world_rotation(kNobody, four);
    EXPECT_EQ(four[3], 1.f);
    EXPECT_NO_FATAL_FAILURE(bindings::native_transform_set_world_position(kNobody, 1.f, 2.f, 3.f));
    EXPECT_NO_FATAL_FAILURE(bindings::native_transform_set_world_rotation(kNobody, 0.f, 0.f, 0.f, 1.f));

    EXPECT_EQ(bindings::native_scene_object_instance(kNobody), nullptr);
    EXPECT_EQ(bindings::native_scene_object_instance(std::numeric_limits<natural_t>::max()), nullptr);
    /// the root is not an object a script holds
    EXPECT_EQ(bindings::native_scene_object_instance(bound.s.root_object().id), nullptr);
    EXPECT_NE(bindings::native_scene_object_instance(bound.hub), nullptr);

    EXPECT_FALSE(bindings::native_scene_set_parent(kNobody, bound.hub, true));
    EXPECT_FALSE(bindings::native_scene_set_parent(bound.spoke, kNobody, true));
    EXPECT_EQ(bindings::native_scene_find_object_with_tag("no-such-tag"), 0u);
  }

  TEST_F(scene_object_tests, a_script_calls_what_is_on_another_object) {
    bound_scene bound;
    EXPECT_EQ(probe("OpenTheHubsDoor"), 1);
    EXPECT_EQ(probe("OpenTheHubsDoor"), 2);
  }

  TEST_F(scene_object_tests, an_object_that_was_destroyed_is_known_to_be) {
    bound_scene bound;
    const size_t before = bound.s.get_object_count();
    EXPECT_EQ(probe("ADestroyedObjectIsKnown"), 1);
    /// only the one made afterwards is left
    EXPECT_EQ(bound.s.get_object_count(), before + 1);
  }

  TEST_F(scene_object_tests, a_component_is_the_same_one_each_time_it_is_asked_for) {
    bound_scene bound;
    EXPECT_EQ(probe("ComponentsAreKept"), 1);
  }

  TEST_F(scene_object_tests, an_object_a_script_holds_is_still_it_after_play_and_stop) {
    bound_scene bound;
    ASSERT_EQ(probe("HoldTheSpoke"), 1);
    EXPECT_EQ(probe("HeldId"), static_cast<int32_t>(bound.spoke & 0x7fffffff));

    bound.s.play();
    bound.s.stop();

    /// stop made every object again, under another id
    const scene_object* spoke = bound.s.find_object(std::string_view{ "Spoke" });
    ASSERT_NE(spoke, nullptr);
    EXPECT_EQ(probe("TheHeldSpokeIsStillTheSpoke"), 1);
    EXPECT_EQ(probe("HeldId"), static_cast<int32_t>(spoke->id & 0x7fffffff));
  }

}  // namespace other
