/**
 * \file tests/scene/scene_tests.cpp
 **/
#include "scene_tests.hpp"

#include <map>

#include "dotnet/dotnet_object.hpp"
#include "script/script_object.hpp"
#include "script/scripting_environment.hpp"

#include "object/script_component.hpp"
#include "scene/scene.hpp"

namespace other {
  namespace {

    script_object* script_object_of(scene& s, natural_t object_id) {
      const script_component* script = s.try_get_component<script_component>(object_id);
      if (script == nullptr || script->script_object_id < 0) {
        return nullptr;
      }
      return subsystem<scripting_environment>::get()->get_object(script->script_object_id);
    }

    dotnet_object* managed_object_of(scene& s, natural_t object_id) {
      script_object* obj = script_object_of(s, object_id);
      return obj != nullptr ? obj->dotnet_object : nullptr;
    }

  }  // namespace

  TEST_F(scene_tests, create_scene_object) {
    scene scene1;

    scene_object& obj1 = scene1.create_object("Test Object 1");
    EXPECT_EQ(obj1.name, "Test Object 1");
    EXPECT_EQ(obj1.id, 1u);

    scene_object& obj2 = scene1.create_object("Test Object 2", &obj1);
    EXPECT_EQ(obj2.name, "Test Object 2");
    EXPECT_GT(obj2.id, 0);

    scene_object* parent_of_obj2 = scene1.get_parent(obj2.id);
    EXPECT_NE(parent_of_obj2, nullptr);
    EXPECT_EQ(parent_of_obj2->id, obj1.id);
  }

  TEST_F(scene_tests, reparent_object_moves_subtree_and_keeps_world_pose) {
    scene s("Reparent Scene");

    scene_object& a = s.create_object("A");
    scene_object& b = s.create_object("B");
    scene_object& c = s.create_object("C", &a);

    transform ta;
    ta.local_position = { 1.f, 2.f, 3.f };
    s.set_transform(a.id, ta);

    transform tc;
    tc.local_position = { 10.f, 0.f, 0.f };
    s.set_transform(c.id, tc);

    const glm::vec3 world_before = glm::vec3(s.get_world_transform(c.id)[3]);
    EXPECT_TRUE(s.reparent_object(c.id, b.id));

    scene_object* parent_of_c = s.get_parent(c.id);
    ASSERT_NE(parent_of_c, nullptr);
    EXPECT_EQ(parent_of_c->id, b.id);
    EXPECT_TRUE(s.get_children_ids(a.id).empty());

    const glm::vec3 world_after = glm::vec3(s.get_world_transform(c.id)[3]);
    EXPECT_NEAR(world_before.x, world_after.x, 1e-4f);
    EXPECT_NEAR(world_before.y, world_after.y, 1e-4f);
    EXPECT_NEAR(world_before.z, world_after.z, 1e-4f);

    /// refused moves: root, self, own descendant
    const natural_t root_id = s.root_object().id;
    EXPECT_FALSE(s.reparent_object(root_id, a.id));
    EXPECT_FALSE(s.reparent_object(b.id, b.id));
    EXPECT_FALSE(s.reparent_object(b.id, c.id));

    /// back to top level via the root
    EXPECT_TRUE(s.reparent_object(c.id, root_id));
    EXPECT_TRUE(s.get_children_ids(b.id).empty());
  }

  TEST_F(scene_tests, scene_move_constructor) {
    scene scene1("Original Scene");                                         //< creates a "Original Scene:Root" object as the root of the scene
    scene_object& obj1 = scene1.create_object("Object in Original Scene");  //< creates a second object
    natural_t obj1_id = obj1.id;

    scene scene2{ std::move(scene1) };

    EXPECT_EQ(scene2.name, "Original Scene");
    EXPECT_EQ(scene2.get_object_count(), 2u);

    scene_object& moved_obj = scene2.get_object(obj1_id);
    EXPECT_EQ(moved_obj.name, "Object in Original Scene");
  }

  TEST_F(scene_tests, scene_move_assignment) {
    scene scene2;
    natural_t obj1_id = 0;

    // this time ensure destructor is called on previous storage
    {
      scene scene1("Original Scene");                                         //< creates a "Original Scene:Root" object as the root of the scene
      scene_object& obj1 = scene1.create_object("Object in Original Scene");  //< creates a second object
      obj1_id = obj1.id;

      scene2 = std::move(scene1);
    }

    EXPECT_EQ(scene2.name, "Original Scene");
    EXPECT_EQ(scene2.get_object_count(), 2u);  // Root object + the object created in the original scene

    scene_object& moved_obj = scene2.get_object(obj1_id);
    EXPECT_EQ(moved_obj.name, "Object in Original Scene");
  }

  TEST_F(scene_tests, scene_move_and_still_works) {
    scene scene1("Original Scene");                                         //< creates a "Original Scene:Root" object as the root of the scene
    scene_object& obj1 = scene1.create_object("Object in Original Scene");  //< creates a second object
    natural_t obj1_id = obj1.id;

    scene scene2 = std::move(scene1);

    EXPECT_EQ(scene2.name, "Original Scene");
    EXPECT_EQ(scene2.get_object_count(), 2u);  // Root object + the object created in the original scene

    scene_object& moved_obj = scene2.get_object(obj1_id);
    EXPECT_EQ(moved_obj.name, "Object in Original Scene");

    scene_object& new_obj = scene2.create_object("New Object in Moved Scene");
    EXPECT_EQ(new_obj.name, "New Object in Moved Scene");
  }

  TEST_F(scene_tests, use_scene_from_std_map) {
    std::map<natural_t, scene> scene_map;
    {
      scene scene1("Mapped Scene");  //< creates a "Mapped Scene:Root" object as the root of the scene
      natural_t scene_id = scene1.id;
      scene_object& _ = scene1.create_object("Object in Mapped Scene");  //< creates a second object
      scene_map.emplace(scene_id, std::move(scene1));
    }

    auto it = scene_map.begin();
    EXPECT_NE(it, scene_map.end());

    scene& mapped_scene = it->second;
    EXPECT_EQ(mapped_scene.name, "Mapped Scene");
    EXPECT_EQ(mapped_scene.get_object_count(), 2u);  // Root object + the second object

    scene_object& mapped_obj = mapped_scene.get_object(it->second.get_all_object_ids().front());
    EXPECT_EQ(mapped_obj.name, "Object in Mapped Scene");
  }

  TEST_F(scene_tests, use_scene_from_std_map2) {
    std::unordered_map<natural_t, scene> scene_map;
    /// this forces a weird constructor order edge case that left the registry in the storage
    ///    referencing the a scene that was destructed bc this constructs a temp and moves it into the map instead of constructing in place
    auto [itr, success] = scene_map.emplace(0, scene("Mapped Scene"));
    ASSERT_TRUE(success);
    ASSERT_TRUE(itr != scene_map.end());

    scene& mapped_scene = itr->second;
    EXPECT_EQ(mapped_scene.name, "Mapped Scene");

    scene_object& _ = itr->second.create_object("Object in Mapped Scene");
  }

  TEST_F(scene_tests, use_scene_from_std_map3) {
    std::unordered_map<natural_t, scene> scene_map;
    /// this forces a weird constructor order edge case that left the registry in the storage
    ///    referencing the a scene that was destructed bc this constructs a temp and moves it into the map instead of constructing in place
    auto [itr, success] = scene_map.emplace(0, "Mapped Scene");
    ASSERT_TRUE(success);
    ASSERT_TRUE(itr != scene_map.end());

    scene& mapped_scene = itr->second;
    EXPECT_EQ(mapped_scene.name, "Mapped Scene");

    scene_object& _ = itr->second.create_object("Object in Mapped Scene");
  }

  TEST_F(scene_tests, use_scene_from_std_vector) {
    std::vector<scene> scene_vector;
    scene& scene = scene_vector.emplace_back("Vector Scene");
    EXPECT_EQ(scene.name, "Vector Scene");

    scene_object& _ = scene.create_object("Object in Vector Scene");
  }

  /// a deactivated scene keeps its objects, so every scene the project has activated is live
  ///  at once; the second scene to use a name used to get no managed object for it
  TEST_F(scene_tests, live_scenes_can_share_object_names) {
    scene first("First Scene");
    scene_object& first_floor = first.create_object("Floor");
    dotnet_object* first_managed = managed_object_of(first, first_floor.id);
    ASSERT_NE(first_managed, nullptr);
    const natural_t first_handle = first_managed->handle;

    {
      scene second("Second Scene");
      scene_object& second_floor = second.create_object("Floor");
      dotnet_object* second_managed = managed_object_of(second, second_floor.id);
      ASSERT_NE(second_managed, nullptr) << "the second scene's 'Floor' lost its managed object to the first";
      EXPECT_NE(second_managed->handle, first_handle);
      EXPECT_NE(second_managed->managed_object, nullptr);
    }

    /// tearing the second scene down must leave the first scene's object alone
    dotnet_object* survivor = managed_object_of(first, first_floor.id);
    ASSERT_NE(survivor, nullptr);
    EXPECT_EQ(survivor->handle, first_handle);
    EXPECT_NE(survivor->managed_object, nullptr);
  }

  /// the editor names every new object "New Object"; the second one used to get no managed object
  TEST_F(scene_tests, objects_in_one_scene_can_share_a_name) {
    scene s("Twin Scene");
    scene_object& first = s.create_object("New Object");
    scene_object& second = s.create_object("New Object");

    dotnet_object* first_managed = managed_object_of(s, first.id);
    dotnet_object* second_managed = managed_object_of(s, second.id);
    ASSERT_NE(first_managed, nullptr);
    ASSERT_NE(second_managed, nullptr);
    EXPECT_NE(first_managed->handle, second_managed->handle);

    s.destroy_object(first.id);
    dotnet_object* survivor = managed_object_of(s, second.id);
    ASSERT_NE(survivor, nullptr);
    EXPECT_EQ(survivor, second_managed);
    EXPECT_NE(survivor->managed_object, nullptr);
  }

  TEST_F(scene_tests, rename_object_updates_the_script_object_label) {
    scene s("Rename Scene");
    scene_object& object = s.create_object("Before");

    script_object* script_obj = script_object_of(s, object.id);
    ASSERT_NE(script_obj, nullptr);
    ASSERT_NE(script_obj->dotnet_object, nullptr);
    EXPECT_EQ(script_obj->name, "Before");
    const natural_t handle = script_obj->dotnet_object->handle;

    s.rename_object(object.id, "After");
    EXPECT_EQ(object.name, "After");
    EXPECT_EQ(script_obj->name, "After");
    EXPECT_EQ(script_obj->dotnet_object->object_name, "After");
    /// a rename relabels the managed object, it does not replace it
    EXPECT_EQ(script_obj->dotnet_object->handle, handle);

    /// renaming onto a name another object holds is allowed
    scene_object& other_object = s.create_object("Taken");
    s.rename_object(object.id, "Taken");
    EXPECT_EQ(object.name, "Taken");
    EXPECT_NE(managed_object_of(s, other_object.id), nullptr);
    EXPECT_NE(managed_object_of(s, object.id), nullptr);

    /// an empty name is refused
    s.rename_object(object.id, "");
    EXPECT_EQ(object.name, "Taken");
  }

}  // namespace other