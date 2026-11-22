/**
 * \file tests/scripting/behavior_lifecycle_tests.cpp
 *
 * the life of a behaviour as the engine keeps it: what it is told and in what order, what
 * switching it off stops, and that one added from C# is the engine's as much as any other
 **/
#include <initializer_list>

#include "script/behavior.hpp"
#include "script/scripting_environment.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    /// FNV-1a over the notes joined by commas, as the probe sums the ones it holds
    int32_t notes_hash(std::initializer_list<std::string_view> notes) {
      uint32_t hash = 2166136261u;
      bool first = true;
      for (const std::string_view note : notes) {
        if (!first) {
          hash = (hash ^ static_cast<uint8_t>(',')) * 16777619u;
        }
        first = false;
        for (const char c : note) {
          hash = (hash ^ static_cast<uint8_t>(c)) * 16777619u;
        }
      }
      return static_cast<int32_t>(hash);
    }

  }  // namespace

  class behavior_lifecycle_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    void SetUp() override {
      other_test::SetUp();
      env = subsystem<scripting_environment>::get();
      ASSERT_NE(env, nullptr);
      ASSERT_NE(get_behavior_runtime(), nullptr);

      parent_id = env->create_object("LifecycleParent");
      ASSERT_GE(parent_id, 0);
      env->attach_dotnet_object(parent_id, "ProbeObject");
      ASSERT_NE(env->get_object(parent_id)->dotnet_object, nullptr);
    }

    void TearDown() override {
      if (env != nullptr && parent_id >= 0) {
        env->destroy_object(parent_id);
      }
      other_test::TearDown();
    }

    int32_t probe(const std::string_view method) {
      return env->call_static_dotnet_method<int32_t>("BehaviorProbe", method);
    }

    /// what the recording behaviour was told, in order, since the last time this was asked
    void expect_notes(std::initializer_list<std::string_view> notes) {
      const int32_t count = probe("Count");
      const int32_t hash = probe("Hash");
      if (count != static_cast<int32_t>(notes.size()) || hash != notes_hash(notes)) {
        probe("Say");
        ADD_FAILURE() << "the behaviour was told " << count << " things where " << notes.size() << " were expected, or others: its notes are in this test's log";
      }
      probe("Clear");
    }

    /// one frame of a scene, as the scene asks for it
    void frame() {
      env->call_behaviors(parent_id, behavior_callback::FIXED_UPDATE);
      env->call_behaviors(parent_id, behavior_callback::UPDATE);
      env->call_behaviors(parent_id, behavior_callback::LATE_UPDATE);
      env->call_behaviors(parent_id, behavior_callback::RENDER);
    }

    const script_object::behavior_handle& behavior(size_t index = 0) {
      return env->get_object(parent_id)->behavior_handles[index];
    }

    scripting_environment* env = nullptr;
    integer_t parent_id = -1;
  };

  TEST_F(behavior_lifecycle_tests, a_behavior_is_told_its_life_in_order) {
    env->attach_dotnet_behavior(parent_id, "RecordingBehavior");
    ASSERT_NE(behavior().instance, nullptr);
    expect_notes({ "awake" });

    /// not playing: only render reaches it
    frame();
    expect_notes({ "render" });

    env->set_object_playing(parent_id, true);
    expect_notes({ "enable" });

    /// start comes once, before the first thing it hears
    frame();
    expect_notes({ "start", "fixed", "update", "late", "render" });
    frame();
    expect_notes({ "fixed", "update", "late", "render" });

    env->set_object_playing(parent_id, false);
    expect_notes({ "disable" });

    /// and again on the next play
    env->set_object_playing(parent_id, true);
    env->call_behaviors(parent_id, behavior_callback::UPDATE);
    expect_notes({ "enable", "start", "update" });

    env->detach_dotnet_behavior(parent_id, "RecordingBehavior");
    expect_notes({ "disable", "destroy" });
    EXPECT_TRUE(env->get_object(parent_id)->behavior_handles.empty());
  }

  TEST_F(behavior_lifecycle_tests, a_disabled_behavior_is_left_alone) {
    env->attach_dotnet_behavior(parent_id, "RecordingBehavior");
    env->set_object_playing(parent_id, true);
    frame();
    probe("Clear");

    ASSERT_TRUE(env->set_behavior_enabled(parent_id, "RecordingBehavior", false));
    expect_notes({ "disable" });
    EXPECT_EQ(probe("RecorderEnabled"), 0);
    frame();
    frame();
    expect_notes({});

    /// switched on again while playing, it starts again
    ASSERT_TRUE(env->set_behavior_enabled(parent_id, "RecordingBehavior", true));
    frame();
    expect_notes({ "enable", "start", "fixed", "update", "late", "render" });

    /// one switched off before play is not told that play began
    env->set_object_playing(parent_id, false);
    env->set_behavior_enabled(parent_id, "RecordingBehavior", false);
    probe("Clear");
    env->set_object_playing(parent_id, true);
    frame();
    expect_notes({});

    EXPECT_FALSE(env->set_behavior_enabled(parent_id, "NoSuchBehavior", true));
  }

  TEST_F(behavior_lifecycle_tests, a_callback_nobody_wrote_is_never_called) {
    env->attach_dotnet_behavior(parent_id, "UpdateOnlyBehavior");
    env->attach_dotnet_behavior(parent_id, "SilentBehavior");
    ASSERT_NE(behavior(0).instance, nullptr);
    ASSERT_NE(behavior(1).instance, nullptr);

    EXPECT_EQ(behavior(0).callbacks, behavior_callback_bit(behavior_callback::UPDATE));
    EXPECT_EQ(behavior(1).callbacks, 0u);

    env->set_object_playing(parent_id, true);
    frame();
    frame();
    EXPECT_EQ(probe("UpdateOnlyUpdates"), 2);
  }

  TEST_F(behavior_lifecycle_tests, what_physics_reports_arrives_with_its_data) {
    env->attach_dotnet_behavior(parent_id, "RecordingBehavior");
    const behavior_contact contact = { .other_object_id = 42, .point = { 1.f, 2.f, 3.f }, .normal = { 0.f, 0.f, -1.f } };
    const float force = 750.f;

    /// not while it is not running
    env->call_behaviors(parent_id, behavior_callback::COLLISION_ENTER, &contact);
    probe("Clear");

    env->set_object_playing(parent_id, true);
    env->call_behaviors(parent_id, behavior_callback::COLLISION_ENTER, &contact);
    env->call_behaviors(parent_id, behavior_callback::JOINT_BREAK, &force);
    expect_notes({ "enable", "start", "hit 42 at 2 facing -1", "broke at 750" });
  }

  TEST_F(behavior_lifecycle_tests, a_behavior_that_throws_is_switched_off) {
    env->attach_dotnet_behavior(parent_id, "ThrowingBehavior");
    env->attach_dotnet_behavior(parent_id, "UpdateOnlyBehavior");
    env->set_object_playing(parent_id, true);

    frame();
    frame();
    frame();
    /// asked once; the behaviour after it is not held up
    EXPECT_EQ(probe("ThrowingUpdates"), 1);
    EXPECT_EQ(probe("UpdateOnlyUpdates"), 3);
    EXPECT_FALSE(behavior(0).enabled);
    EXPECT_FALSE(behavior(0).running);
    EXPECT_TRUE(behavior(1).running);
  }

  TEST_F(behavior_lifecycle_tests, a_behavior_can_switch_itself_off_from_a_callback) {
    env->attach_dotnet_behavior(parent_id, "SelfDisablingBehavior");
    env->set_object_playing(parent_id, true);
    frame();
    frame();
    EXPECT_EQ(probe("SelfDisablingUpdates"), 1);
    EXPECT_EQ(probe("SelfDisablingDisables"), 1);
    EXPECT_FALSE(behavior().enabled);
  }

  TEST_F(behavior_lifecycle_tests, a_behavior_made_while_waking_is_deferred_wakes_when_it_ends) {
    env->defer_behavior_awake(true);
    env->attach_dotnet_behavior(parent_id, "RecordingBehavior");
    ASSERT_NE(behavior().instance, nullptr);
    EXPECT_FALSE(behavior().awake);
    expect_notes({});

    env->defer_behavior_awake(false);
    EXPECT_TRUE(behavior().awake);
    expect_notes({ "awake" });
  }

  TEST_F(behavior_lifecycle_tests, a_behavior_added_from_csharp_is_the_engines_too) {
    EXPECT_EQ(probe("AddRecorder"), 1);
    expect_notes({ "awake" });

    /// in the engine's record, which is what is saved and made again after a rebuild
    ASSERT_EQ(env->get_object(parent_id)->behavior_handles.size(), 1u);
    EXPECT_NE(behavior().instance, nullptr);
    EXPECT_EQ(probe("BehaviorCount"), 1);

    /// one of a type on an object
    EXPECT_EQ(probe("AddRecorderTwice"), 1);
    EXPECT_EQ(env->get_object(parent_id)->behavior_handles.size(), 1u);

    /// its switch is the engine's
    env->set_object_playing(parent_id, true);
    probe("Clear");
    EXPECT_EQ(env->call_static_dotnet_method<int32_t>("BehaviorProbe", "SetRecorderEnabled", int32_t{ 0 }), 1);
    EXPECT_FALSE(behavior().enabled);
    expect_notes({ "disable" });

    EXPECT_EQ(probe("RemoveRecorder"), 1);
    EXPECT_TRUE(env->get_object(parent_id)->behavior_handles.empty());
    EXPECT_EQ(probe("HasRecorder"), 0);
    expect_notes({ "destroy" });
    EXPECT_EQ(probe("RemoveRecorder"), 0);
  }

  TEST_F(behavior_lifecycle_tests, one_added_while_its_object_plays_begins_running) {
    env->set_object_playing(parent_id, true);
    EXPECT_EQ(probe("AddRecorder"), 1);
    env->call_behaviors(parent_id, behavior_callback::UPDATE);
    expect_notes({ "awake", "enable", "start", "update" });
  }

  TEST_F(behavior_lifecycle_tests, an_object_that_goes_tells_its_behaviors) {
    env->attach_dotnet_behavior(parent_id, "RecordingBehavior");
    env->set_object_playing(parent_id, true);
    probe("Clear");

    env->destroy_object(parent_id);
    parent_id = -1;
    expect_notes({ "disable", "destroy" });
  }

}  // namespace other
