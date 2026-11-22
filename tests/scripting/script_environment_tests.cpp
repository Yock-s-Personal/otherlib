/**
 * \file tests/scripting/script_environment_tests.cpp
 **/
#include "script_environment_tests.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "dotnet/dotnet_object.hpp"
#include "script/behavior_fields.hpp"

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
    EXPECT_NE(parent->behavior_handles[0].instance, nullptr);
    EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 1);

    dotnet_type* behavior_type = env->get_dotnet_host().get_type_cache()->get_type("TestBehavior");
    ASSERT_NE(behavior_type, nullptr);

    /// unload half (mirrors assembly_context::unload_assembly's per-type sequence)
    EXPECT_NO_FATAL_FAILURE(env->invalidate_dotnet_script_objects_of_type(behavior_type->dotnet_id));
    EXPECT_NO_FATAL_FAILURE(env->get_dotnet_host().purge_dotnet_type(behavior_type->dotnet_id));

    ASSERT_EQ(parent->behavior_handles.size(), 1u);
    EXPECT_EQ(parent->behavior_handles[0].instance, nullptr);
    EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 0);

    /// reload half
    EXPECT_NO_FATAL_FAILURE(env->reattach_invalidated_dotnet_behaviors());
    ASSERT_EQ(parent->behavior_handles.size(), 1u);
    EXPECT_NE(parent->behavior_handles[0].instance, nullptr);
    EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 1);

    EXPECT_NO_FATAL_FAILURE(env->detach_all_dotnet_behaviors(parent_id));
    EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 0);
    EXPECT_NO_FATAL_FAILURE(env->destroy_object(parent_id));
  }

  /// what the behavior inspector draws from: the runtime says what a type's saved fields
  ///  are, and their values go out and come back as a stream
  TEST_F(script_environment_tests, behavior_description_lists_fields_and_values_roundtrip) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);

    integer_t parent_id = -1;
    EXPECT_NO_FATAL_FAILURE(parent_id = env->create_object("DescribedFieldsParent"));
    ASSERT_GE(parent_id, 0);
    EXPECT_NO_FATAL_FAILURE(env->attach_dotnet_object(parent_id, "TestParentObject"));
    EXPECT_NO_FATAL_FAILURE(env->attach_dotnet_behavior(parent_id, "TestBehavior"));

    const ostd::vector<behavior_field_description>& described = env->describe_behavior(parent_id, "TestBehavior");
    const auto kind_of = [&described](const std::string_view name) -> behavior_field_kind {
      const auto found = std::ranges::find(described, name, &behavior_field_description::name);
      return found != described.end() ? found->kind : behavior_field_kind::NONE;
    };
    EXPECT_EQ(described.size(), 4u);
    EXPECT_EQ(kind_of("speed"), behavior_field_kind::FLOAT);
    EXPECT_EQ(kind_of("counter"), behavior_field_kind::INT32);
    EXPECT_EQ(kind_of("active"), behavior_field_kind::BOOL);
    EXPECT_EQ(kind_of("label"), behavior_field_kind::TEXT);
    /// asked once for a type: the same list is handed back
    EXPECT_EQ(&env->describe_behavior(parent_id, "TestBehavior"), &described);
    EXPECT_TRUE(env->describe_behavior(parent_id, "NoSuchBehavior").empty());

    ostd::vector<uint8_t> stream = {};
    ostd::vector<behavior_fields::field> fields = {};
    const auto field = [&](const std::string_view name) -> const behavior_fields::field* {
      stream = env->capture_behavior_fields(parent_id, "TestBehavior");
      if (!behavior_fields::read(stream, fields)) {
        return nullptr;
      }
      const auto found = std::ranges::find(fields, name, &behavior_fields::field::name);
      return found != fields.end() ? &*found : nullptr;
    };

    float speed = 0.f;
    ASSERT_NE(field("speed"), nullptr);
    EXPECT_TRUE(field("speed")->value(speed));
    EXPECT_FLOAT_EQ(speed, 2.5f);
    EXPECT_EQ(field("label")->text(), "steady");

    /// one field given a value, as the inspector does it: the others are left
    behavior_fields::writer edit = {};
    edit.add_value<float>("speed", behavior_field_kind::FLOAT, 11.25f);
    env->apply_behavior_fields(parent_id, "TestBehavior", edit.take());
    EXPECT_TRUE(field("speed")->value(speed));
    EXPECT_FLOAT_EQ(speed, 11.25f);
    EXPECT_EQ(field("label")->text(), "steady");
    int32_t counter = 0;
    EXPECT_TRUE(field("counter")->value(counter));
    EXPECT_EQ(counter, 7);

    /// text shorter, then longer, than what was there
    for (const std::string_view label : { std::string_view{ "ok" }, std::string_view{ "a longer label than before" } }) {
      edit.add_text("label", behavior_field_kind::TEXT, label);
      env->apply_behavior_fields(parent_id, "TestBehavior", edit.take());
      EXPECT_EQ(field("label")->text(), label);
    }

    EXPECT_NO_FATAL_FAILURE(env->destroy_object(parent_id));
  }

  /// snapshot restore (undo/redo, play/stop) destroys the parent slot and rebuilds it; its
  ///  behaviors' managed objects must go with it or every rebuild leaks one
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

      EXPECT_NE(parent->behavior_handles[0].managed, nullptr) << "cycle " << cycle;
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
    EXPECT_EQ(parent->behavior_handles[0].instance, nullptr);
    EXPECT_EQ(parent->dotnet_object->invoke<int>("CountBehaviors"), 0);

    /// attach stays idempotent per type, and a refresh that still lacks the type changes nothing
    env->attach_dotnet_behavior(parent_id, "NoSuchBehaviorType");
    EXPECT_EQ(parent->behavior_handles.size(), 1u);
    env->reattach_invalidated_dotnet_behaviors();
    ASSERT_EQ(parent->behavior_handles.size(), 1u);
    EXPECT_EQ(parent->behavior_handles[0].instance, nullptr);

    /// a type that exists attaches beside it and the unresolved one can still be removed
    env->attach_dotnet_behavior(parent_id, "TestBehavior");
    ASSERT_EQ(parent->behavior_handles.size(), 2u);
    EXPECT_NE(parent->behavior_handles[1].instance, nullptr);

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
    const dotnet_object* behavior = obj->behavior_handles[0].managed;
    ASSERT_NE(behavior, nullptr);
    EXPECT_EQ(behavior->object_name, "Before__behavior__TestBehavior");
    const natural_t handle = obj->dotnet_object->handle;

    env->rename_object(id, "After");

    EXPECT_EQ(obj->name, "After");
    EXPECT_EQ(obj->dotnet_object->object_name, "After");
    EXPECT_EQ(obj->dotnet_object->handle, handle);
    EXPECT_EQ(behavior->object_name, "After__behavior__TestBehavior");
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