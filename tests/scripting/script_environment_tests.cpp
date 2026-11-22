/**
 * \file tests/scripting/script_environment_tests.cpp
 **/
#include "script_environment_tests.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "dotnet/dotnet_object.hpp"

#include "gtest/gtest.h"

namespace other {

  TEST_F(script_environment_tests, call_static_dotnet_method) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);

    {
      integer_t obj_id = 0;
      EXPECT_NO_FATAL_FAILURE(obj_id = env->create_object("MyObject"));
      ASSERT_GE(obj_id, 0);

      script_object* script_obj = nullptr;
      EXPECT_NO_FATAL_FAILURE(script_obj = env->get_object(obj_id));
      ASSERT_NE(script_obj, nullptr);
      EXPECT_EQ(script_obj->dotnet_object, nullptr);

      EXPECT_NO_FATAL_FAILURE(env->attach_dotnet_object(obj_id, "TestObject"));
      EXPECT_NO_FATAL_FAILURE(script_obj = env->get_object(obj_id));
      ASSERT_NE(script_obj, nullptr);
      ASSERT_NE(script_obj->dotnet_object, nullptr);

      int res = -1;
      EXPECT_NO_FATAL_FAILURE(res = script_obj->dotnet_object->invoke<int>("StaticMethod", int{ 5 }));
      EXPECT_EQ(res, 25);

      EXPECT_NO_FATAL_FAILURE(env->destroy_object(obj_id));
    }

    {
      int res = -1;
      EXPECT_NO_FATAL_FAILURE(res = env->call_static_dotnet_method<int>("TestObject", "StaticMethod", int{ 6 }));
      EXPECT_EQ(res, 36);
    }
  }

  /// unload/reload halves of an assembly refresh: invalidation must unhook the behavior from its
  ///  parent's C# behavior list (not just native records), or reattach resurrects stale instances
  TEST_F(script_environment_tests, behavior_invalidate_and_reattach) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);

    integer_t parent_id = -1;
    EXPECT_NO_FATAL_FAILURE(parent_id = env->create_object("HotReloadParent"));
    ASSERT_GE(parent_id, 0);
    EXPECT_NO_FATAL_FAILURE(env->attach_dotnet_object(parent_id, "TestParentObject"));

    script_object* parent = env->get_object(parent_id);
    ASSERT_NE(parent, nullptr);
    ASSERT_NE(parent->dotnet_object, nullptr);

    EXPECT_NO_FATAL_FAILURE(env->attach_dotnet_behavior(parent_id, "TestBehavior"));
    ASSERT_EQ(parent->behavior_handles.size(), 1u);
    EXPECT_GE(parent->behavior_handles[0].script_object_id, 0);
    EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 1);

    dotnet_type* behavior_type = env->get_dotnet_host().get_type_cache()->get_type("TestBehavior");
    ASSERT_NE(behavior_type, nullptr);

    /// unload half (mirrors assembly_context::unload_assembly's per-type sequence)
    EXPECT_NO_FATAL_FAILURE(env->invalidate_dotnet_script_objects_of_type(behavior_type->dotnet_id));
    EXPECT_NO_FATAL_FAILURE(env->get_dotnet_host().purge_dotnet_type(behavior_type->dotnet_id));

    ASSERT_EQ(parent->behavior_handles.size(), 1u);
    EXPECT_EQ(parent->behavior_handles[0].script_object_id, -1);
    EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 0);

    /// reload half
    EXPECT_NO_FATAL_FAILURE(env->reattach_invalidated_dotnet_behaviors());
    ASSERT_EQ(parent->behavior_handles.size(), 1u);
    EXPECT_GE(parent->behavior_handles[0].script_object_id, 0);
    EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 1);

    EXPECT_NO_FATAL_FAILURE(env->detach_all_dotnet_behaviors(parent_id));
    EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 0);
    EXPECT_NO_FATAL_FAILURE(env->destroy_object(parent_id));
  }

  /// managed-reflection pipeline behind the behavior inspector: GetFields/GetProperties must
  ///  enumerate public members (BindingFlags regression); snapshot walks each behavior's own type
  TEST_F(script_environment_tests, behavior_snapshot_lists_fields_and_roundtrips) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);

    integer_t parent_id = -1;
    EXPECT_NO_FATAL_FAILURE(parent_id = env->create_object("SnapshotFieldsParent"));
    ASSERT_GE(parent_id, 0);
    EXPECT_NO_FATAL_FAILURE(env->attach_dotnet_object(parent_id, "TestParentObject"));

    script_object* parent = env->get_object(parent_id);
    ASSERT_NE(parent, nullptr);
    ASSERT_NE(parent->dotnet_object, nullptr);
    EXPECT_NO_FATAL_FAILURE(env->attach_dotnet_behavior(parent_id, "TestBehavior"));

    behavior_snapshot snapshot = parent->get_behavior_snapshot();
    ASSERT_TRUE(snapshot.valid);
    ASSERT_EQ(snapshot.behaviors.size(), 1u);

    const behavior_descriptor& behavior = snapshot.behaviors[0];
    EXPECT_EQ(behavior.display_name, "TestBehavior");
    ASSERT_GE(behavior.script_object_id, 0);

    auto find_field = [&](std::string_view name) -> const behavior_field_descriptor* {
      for (const auto& f : behavior.fields) {
        if (f.field_name == name) {
          return &f;
        }
      }
      return nullptr;
    };

    const behavior_field_descriptor* speed = find_field("speed");
    ASSERT_NE(speed, nullptr);
    EXPECT_EQ(speed->type, value_type::FLOAT);
    const behavior_field_descriptor* counter = find_field("counter");
    ASSERT_NE(counter, nullptr);
    EXPECT_EQ(counter->type, value_type::INT32);
    const behavior_field_descriptor* active = find_field("active");
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->type, value_type::OEBOOL);
    const behavior_field_descriptor* label = find_field("label");
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->type, value_type::STRING);

    script_object* behavior_obj = env->get_object(behavior.script_object_id);
    ASSERT_NE(behavior_obj, nullptr);
    ASSERT_NE(behavior_obj->dotnet_object, nullptr);

    float speed_val = 0.f;
    EXPECT_EQ(behavior_obj->dotnet_object->read_field_value("speed", &speed_val, sizeof(speed_val)), (int32_t)sizeof(float));
    EXPECT_FLOAT_EQ(speed_val, 2.5f);

    const float new_speed = 11.25f;
    EXPECT_TRUE(behavior_obj->dotnet_object->write_field_value("speed", &new_speed, sizeof(new_speed)));
    speed_val = 0.f;
    behavior_obj->dotnet_object->read_field_value("speed", &speed_val, sizeof(speed_val));
    EXPECT_FLOAT_EQ(speed_val, 11.25f);

    char text[64] = {};
    int32_t text_len = behavior_obj->dotnet_object->read_field_value("label", text, sizeof(text));
    EXPECT_EQ(std::string(text), "steady");
    EXPECT_EQ(text_len, (int32_t)std::string("steady").size() + 1);

    /// shrink then grow, the storage block reuse path both ways
    const char* short_label = "ok";
    EXPECT_TRUE(behavior_obj->dotnet_object->write_field_value("label", short_label, (int32_t)std::strlen(short_label) + 1));
    std::memset(text, 0, sizeof(text));
    behavior_obj->dotnet_object->read_field_value("label", text, sizeof(text));
    EXPECT_EQ(std::string(text), "ok");

    const char* long_label = "a longer label than before";
    EXPECT_TRUE(behavior_obj->dotnet_object->write_field_value("label", long_label, (int32_t)std::strlen(long_label) + 1));
    std::memset(text, 0, sizeof(text));
    behavior_obj->dotnet_object->read_field_value("label", text, sizeof(text));
    EXPECT_EQ(std::string(text), long_label);

    EXPECT_NO_FATAL_FAILURE(env->destroy_object(parent_id));
  }

  /// snapshot restore (undo/redo, play/stop) destroys the parent slot and rebuilds it; behavior
  ///  slots must release with it or every rebuild leaks a managed object
  TEST_F(script_environment_tests, destroy_parent_releases_behaviors_for_rebuild) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);
    const size_t baseline = env->get_dotnet_host().managed_object_count();

    for (int cycle = 0; cycle < 3; ++cycle) {
      integer_t parent_id = -1;
      EXPECT_NO_FATAL_FAILURE(parent_id = env->create_object("SnapshotParent"));
      ASSERT_GE(parent_id, 0);
      EXPECT_NO_FATAL_FAILURE(env->attach_dotnet_object(parent_id, "TestParentObject"));

      script_object* parent = env->get_object(parent_id);
      ASSERT_NE(parent, nullptr);
      ASSERT_NE(parent->dotnet_object, nullptr);

      EXPECT_NO_FATAL_FAILURE(env->attach_dotnet_behavior(parent_id, "TestBehavior"));
      ASSERT_EQ(parent->behavior_handles.size(), 1u) << "cycle " << cycle;

      const integer_t behavior_id = parent->behavior_handles[0].script_object_id;
      ASSERT_GE(behavior_id, 0) << "cycle " << cycle;
      script_object* behavior = env->get_object(behavior_id);
      ASSERT_NE(behavior, nullptr);
      EXPECT_NE(behavior->dotnet_object, nullptr) << "behavior slot should own its managed object";
      EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 1);

      /// scene teardown destroys only the parent's slot
      EXPECT_NO_FATAL_FAILURE(env->destroy_object(parent_id));
      /// names no longer collide, so a leaked behavior shows up as a count that keeps growing
      EXPECT_EQ(env->get_dotnet_host().managed_object_count(), baseline) << "cycle " << cycle;
    }
  }

  /// managed objects are keyed by a handle, so a name is free to repeat: across scenes and
  ///  inside one. the second object used to fail with "already exists"
  TEST_F(script_environment_tests, same_named_objects_each_own_a_managed_object) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);
    const size_t baseline = env->get_dotnet_host().managed_object_count();

    const integer_t first_id = env->create_object("Twin");
    const integer_t second_id = env->create_object("Twin");
    ASSERT_GE(first_id, 0);
    ASSERT_GE(second_id, 0);
    ASSERT_NE(first_id, second_id);

    env->attach_dotnet_object(first_id, "TestParentObject");
    env->attach_dotnet_object(second_id, "TestParentObject");

    script_object* first = env->get_object(first_id);
    script_object* second = env->get_object(second_id);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(first->dotnet_object, nullptr);
    ASSERT_NE(second->dotnet_object, nullptr) << "the second object lost its managed object to the shared name";
    EXPECT_NE(first->dotnet_object, second->dotnet_object);
    EXPECT_NE(first->dotnet_object->handle, second->dotnet_object->handle);
    EXPECT_EQ(env->get_dotnet_host().managed_object_count(), baseline + 2);

    /// a behavior is labelled after its parent, so its label repeats too
    env->attach_dotnet_behavior(first_id, "TestBehavior");
    env->attach_dotnet_behavior(second_id, "TestBehavior");
    ASSERT_EQ(first->behavior_handles.size(), 1u);
    ASSERT_EQ(second->behavior_handles.size(), 1u);
    EXPECT_EQ(first->dotnet_object->invoke<int>("CountBehaviors"), 1);
    EXPECT_EQ(second->dotnet_object->invoke<int>("CountBehaviors"), 1);
    EXPECT_EQ(env->get_dotnet_host().managed_object_count(), baseline + 4);

    /// destroying one must leave the other's managed objects alone
    env->destroy_object(first_id);
    ASSERT_NE(second->dotnet_object, nullptr);
    EXPECT_EQ(second->dotnet_object->invoke<int>("CountBehaviors"), 1);
    EXPECT_EQ(env->get_dotnet_host().managed_object_count(), baseline + 2);

    env->destroy_object(second_id);
    EXPECT_EQ(env->get_dotnet_host().managed_object_count(), baseline);
  }

  /// a parent can lack its managed object (type missing, constructor threw); the behavior is
  ///  dropped with an error. this used to abort the process
  TEST_F(script_environment_tests, behavior_attach_without_managed_parent_is_not_fatal) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);

    const integer_t parent_id = env->create_object("Unmanaged");
    ASSERT_GE(parent_id, 0);
    script_object* parent = env->get_object(parent_id);
    ASSERT_NE(parent, nullptr);
    ASSERT_EQ(parent->dotnet_object, nullptr);

    env->attach_dotnet_behavior(parent_id, "TestBehavior");
    EXPECT_TRUE(parent->behavior_handles.empty());

    env->detach_dotnet_behavior(parent_id, "TestBehavior");
    EXPECT_TRUE(parent->behavior_handles.empty());

    env->destroy_object(parent_id);
  }

  /// a missing type (the project assembly failed to build) leaves an invalid handle, not
  ///  nothing: scene capture reads behavior names from the handles, so dropping it would
  ///  erase the behavior from the next saved scene
  TEST_F(script_environment_tests, behavior_with_missing_type_keeps_an_invalid_handle) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);

    const integer_t parent_id = env->create_object("MissingTypeParent");
    ASSERT_GE(parent_id, 0);
    env->attach_dotnet_object(parent_id, "TestParentObject");
    script_object* parent = env->get_object(parent_id);
    ASSERT_NE(parent, nullptr);
    ASSERT_NE(parent->dotnet_object, nullptr);

    env->attach_dotnet_behavior(parent_id, "NoSuchBehaviorType");
    ASSERT_EQ(parent->behavior_handles.size(), 1u);
    EXPECT_EQ(parent->behavior_handles[0].type_name, "NoSuchBehaviorType");
    EXPECT_EQ(parent->behavior_handles[0].script_object_id, -1);
    EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 0);

    /// attach stays idempotent per type, and a refresh that still lacks the type changes nothing
    env->attach_dotnet_behavior(parent_id, "NoSuchBehaviorType");
    EXPECT_EQ(parent->behavior_handles.size(), 1u);
    env->reattach_invalidated_dotnet_behaviors();
    ASSERT_EQ(parent->behavior_handles.size(), 1u);
    EXPECT_EQ(parent->behavior_handles[0].script_object_id, -1);

    /// a type that exists attaches beside it and the unresolved one can still be removed
    env->attach_dotnet_behavior(parent_id, "TestBehavior");
    ASSERT_EQ(parent->behavior_handles.size(), 2u);
    EXPECT_GE(parent->behavior_handles[1].script_object_id, 0);

    env->detach_dotnet_behavior(parent_id, "NoSuchBehaviorType");
    ASSERT_EQ(parent->behavior_handles.size(), 1u);
    EXPECT_EQ(parent->behavior_handles[0].type_name, "TestBehavior");
    EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 1);

    env->destroy_object(parent_id);
  }

  /// a name is a label: renaming updates it everywhere without replacing the managed object
  TEST_F(script_environment_tests, rename_updates_object_and_behavior_labels) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);

    const integer_t id = env->create_object("Before");
    ASSERT_GE(id, 0);
    env->attach_dotnet_object(id, "TestParentObject");
    env->attach_dotnet_behavior(id, "TestBehavior");

    script_object* obj = env->get_object(id);
    ASSERT_NE(obj, nullptr);
    ASSERT_NE(obj->dotnet_object, nullptr);
    ASSERT_EQ(obj->behavior_handles.size(), 1u);
    script_object* behavior = env->get_object(obj->behavior_handles[0].script_object_id);
    ASSERT_NE(behavior, nullptr);
    ASSERT_NE(behavior->dotnet_object, nullptr);
    EXPECT_EQ(behavior->name, "Before__behavior__TestBehavior");
    const natural_t handle = obj->dotnet_object->handle;

    env->rename_object(id, "After");

    EXPECT_EQ(obj->name, "After");
    EXPECT_EQ(obj->dotnet_object->object_name, "After");
    EXPECT_EQ(obj->dotnet_object->handle, handle);
    EXPECT_EQ(behavior->name, "After__behavior__TestBehavior");
    EXPECT_EQ(behavior->dotnet_object->object_name, "After__behavior__TestBehavior");
    EXPECT_EQ(obj->dotnet_object->invoke<int>("CountBehaviors"), 1);

    env->destroy_object(id);
  }

  /// a build caught half way leaves a file that is not an assembly: that is a failed load
  ///  which keeps nothing, so the finished file loads from the same path afterwards
  TEST_F(script_environment_tests, a_file_that_is_not_an_assembly_fails_to_load_and_its_path_loads_later) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);

    std::ifstream in(testing_dll, std::ios::binary);
    ASSERT_TRUE(in.is_open()) << testing_dll.string();
    const std::vector<char> whole{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
    in.close();
    ASSERT_GT(whole.size(), 1024u);

    const filepath folder = std::filesystem::temp_directory_path() / "other-script-environment-tests";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    const filepath rebuilt = folder / "DotnetTesting.dll";
    const auto write = [&rebuilt, &whole](const size_t bytes) -> void {
      std::ofstream out(rebuilt, std::ios::binary | std::ios::trunc);
      out.write(whole.data(), static_cast<std::streamsize>(bytes));
    };

    write(whole.size() / 2);
    EXPECT_EQ(env->load_dotnet_module(rebuilt.string()), nullptr);
    /// and again: the first failure did not leave the path taken
    EXPECT_EQ(env->load_dotnet_module(rebuilt.string()), nullptr);

    /// an assembly's name loads once, so the fixture's copy gives its place to the rebuilt one
    write(whole.size());
    env->unload_dotnet_module(env->get_dotnet_module("DotnetTesting"));
    const ref<assembly> reloaded = env->load_dotnet_module(rebuilt.string());
    EXPECT_NE(reloaded, nullptr) << "the whole assembly did not load from the path a broken one was at";
    if (reloaded != nullptr) {
      EXPECT_EQ(env->call_static_dotnet_method<int>("TestObject", "StaticMethod", int{ 6 }), 36);
      env->unload_dotnet_module(reloaded);
    }
    /// back where the fixture expects its copy, which TearDown unloads by its path
    ASSERT_NE(env->load_dotnet_module(testing_dll.string()), nullptr);

    std::error_code ec;
    std::filesystem::remove_all(folder, ec);
  }

}  // namespace other