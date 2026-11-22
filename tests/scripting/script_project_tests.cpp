/**
 * \file tests/scripting/script_project_tests.cpp
 *
 * the open project as C# reads it: every part arrives as it is, and the strings are only
 * read again when something about the project changed
 **/
#include "script/scripting_environment.hpp"

#include "project/project.hpp"
#include "scripting/dotnet_bindings/project_bindings.hpp"

#include "other_test.hpp"

namespace other {

  class script_project_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    void SetUp() override {
      other_test::SetUp();
      env = subsystem<scripting_environment>::get();
      ASSERT_NE(env, nullptr);

      current.state = project::LOADED;
      current.name = "Sandbox Game";
      current.version = "0.1.0";
      current.author = "Someone";
      current.description = "a game, tested";
      current.file = "game/sandbox.toml";
      current.scene_count = 2;
      current.starting_scene = "Main";
      current.file_notice = true;
      current.file_notice_needs_reopen = { "the scene list", "the plugins" };
      current.load_errors = { { .subject = "Second", .message = "its file is missing" } };
      current.script_project_path = "game/Game.csproj";
      current.scripts_building = true;
      current.scripts_loaded = false;
      current.script_failure = true;
      current.script_failure_project_path = "game/Game.csproj";
      current.script_failure_reason = "The build failed.";
      current.script_failure_details = "error CS0103: caf\xc3\xa9 does not exist";
      current.script_failure_can_retry = false;
      bindings::set_script_project(&current);
    }

    void TearDown() override {
      bindings::set_script_project(nullptr);
      other_test::TearDown();
    }

    int32_t probe(const std::string_view method) {
      return env->call_static_dotnet_method<int32_t>("ProjectProbe", method);
    }

    scripting_environment* env = nullptr;
    bindings::script_project current;
  };

  TEST_F(script_project_tests, every_part_of_the_project_reads_as_it_is) {
    EXPECT_EQ(probe("Read"), 127);
  }

  TEST_F(script_project_tests, a_state_is_the_same_number_on_both_sides) {
    const project::state states[] = { project::EMPTY, project::LOADING, project::LOADED, project::LOAD_FAILED, project::UNLOADING };
    for (int32_t which = 0; which < 5; ++which) {
      current.state = states[which];
      EXPECT_EQ(env->call_static_dotnet_method<int32_t>("ProjectProbe", "StateIs", which), 1) << which;
    }

    current.state = project::EMPTY;
    EXPECT_EQ(probe("IsOpen"), 0);
  }

  TEST_F(script_project_tests, strings_are_read_again_only_when_the_project_changed) {
    ASSERT_EQ(probe("Read"), 127);
    EXPECT_EQ(probe("RevisionMoved"), 1);
    const natural_t read_once = bindings::script_project_string_reads();
    EXPECT_GT(read_once, 0u);

    /// the same project, asked about again and again: nothing crosses but the stamp
    for (int32_t frame = 0; frame < 10; ++frame) {
      EXPECT_EQ(probe("Read"), 127);
    }
    EXPECT_EQ(probe("RevisionMoved"), 0);
    EXPECT_EQ(bindings::script_project_string_reads(), read_once);

    /// one letter of one string is a change
    current.name = "Another Name";
    EXPECT_EQ(probe("NameIsRenamed"), 1);
    EXPECT_EQ(probe("RevisionMoved"), 1);
    EXPECT_GT(bindings::script_project_string_reads(), read_once);

    /// and so is text of the same length in the build's output
    const natural_t before = bindings::script_project_string_reads();
    current.script_failure_details = "error CS0103: caf\xc3\xa9 does not exisT";
    EXPECT_EQ(probe("RevisionMoved"), 1);
    EXPECT_GT(bindings::script_project_string_reads(), before);
  }

  TEST_F(script_project_tests, a_failure_that_is_gone_reads_as_none) {
    ASSERT_EQ(probe("HasNoFailure"), 0);
    current.script_failure = false;
    EXPECT_EQ(probe("HasNoFailure"), 1);
  }

  TEST_F(script_project_tests, a_rebuild_asked_for_in_csharp_reaches_the_engine) {
    const natural_t before = bindings::script_project_rebuild_requests();
    EXPECT_EQ(probe("RequestRebuild"), 1);
    EXPECT_EQ(bindings::script_project_rebuild_requests(), before + 1);
  }

  TEST_F(script_project_tests, two_projects_have_the_same_stamp_only_when_they_read_the_same) {
    bindings::script_project other_project = current;
    EXPECT_EQ(other_project.stamp(), current.stamp());

    /// the same letters split differently between two fields
    other_project.name = "Sandbox Gam";
    other_project.version = "e0.1.0";
    EXPECT_NE(other_project.stamp(), current.stamp());

    other_project = current;
    other_project.script_failure_can_retry = true;
    EXPECT_NE(other_project.stamp(), current.stamp());

    other_project = current;
    other_project.load_errors.push_back({ .subject = "", .message = "" });
    EXPECT_NE(other_project.stamp(), current.stamp());
  }

}  // namespace other
