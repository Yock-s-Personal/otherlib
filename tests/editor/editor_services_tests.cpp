/**
 * \file tests/editor/editor_services_tests.cpp
 *
 * what an editor window works with that is not the scene itself: the selection and the
 * edits that can be undone, natively and through the editor's C# API (Other.Editor)
 **/
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "script/scripting_environment.hpp"

#include "object/script_component.hpp"
#include "scene/scene.hpp"

#include "scripting/dotnet_bindings/scene_bindings.hpp"
#include "scripting/other_abi.hpp"

#include "other_test.hpp"
#include "services/editor_bindings.hpp"
#include "services/editor_services.hpp"

namespace other {

  class editor_services_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    /// a scene the editor's services and the script bindings act on for as long as it
    ///  lives: "First" and "Second" at the top, "Under" under "Second"
    struct edited_scene {
      scene s{ "Editor Scene" };
      editor_services services;
      natural_t first = 0;
      natural_t second = 0;
      natural_t under = 0;

      edited_scene() {
        first = s.create_object("First").id;
        scene_object& second_object = s.create_object("Second");
        second = second_object.id;
        under = s.create_object("Under", &second_object).id;

        services.current_selection.scene_ptr = &s;
        detail::set_scene_for_bindings(&s);
        bindings::set_editor_services(&services);
      }

      ~edited_scene() {
        bindings::set_editor_services(nullptr);
        detail::set_scene_for_bindings(nullptr);
      }

      edited_scene(const edited_scene&) = delete;
      edited_scene& operator=(const edited_scene&) = delete;

      /// frames in which no widget changes anything, enough for an open edit to be recorded
      void quiet_frames() {
        for (uint32_t i = 0; i <= editor_services::scene_edit_tracker::kIdleFramesToCommit; ++i) {
          services.tick_edit_tracker();
        }
      }
    };

    void SetUp() override {
      other_test::SetUp();
      env = subsystem<scripting_environment>::get();
      ASSERT_NE(env, nullptr);

      /// the editor's API is an assembly of its own, loaded by the editor and bound by it
      api = env->load_dotnet_module(perform_tag_replacement("build/other-editor/api/${build-config}/OtherEditor.dll"));
      ASSERT_NE(api, nullptr);
      ASSERT_EQ(bindings::bind_editor_functions(env->get_dotnet_host()), 0u);
    }

    void TearDown() override {
      if (api != nullptr) {
        env->unload_dotnet_module(api);
        api = nullptr;
      }
      other_test::TearDown();
    }

    int32_t probe(const std::string_view method) {
      return env->call_static_dotnet_method<int32_t>("EditorProbe", method);
    }

    scripting_environment* env = nullptr;
    ref<assembly> api = nullptr;
    const filepath testing_dll = perform_tag_replacement("build/script-testing/${build-config}/DotnetTesting.dll");
  };

  TEST_F(editor_services_tests, the_selection_is_set_added_to_and_cleared) {
    edited_scene edited;
    editor_services& services = edited.services;
    EXPECT_FALSE(services.has_selection());
    EXPECT_EQ(services.active_selection(), 0u);

    const uint64_t at_start = services.selection_revision();
    services.set_selection(edited.first);
    EXPECT_EQ(services.active_selection(), edited.first);
    EXPECT_NE(services.selection_revision(), at_start);

    services.add_to_selection(edited.second);
    services.add_to_selection(edited.first);
    ASSERT_EQ(services.current_selection.objects.size(), 2u);
    EXPECT_EQ(services.active_selection(), edited.first);

    /// what is no object is not selected, and nothing counts as a change
    const uint64_t before = services.selection_revision();
    services.add_to_selection(4000);
    services.add_to_selection(0);
    EXPECT_EQ(services.current_selection.objects.size(), 2u);
    EXPECT_EQ(services.selection_revision(), before);

    services.remove_from_selection(edited.first);
    EXPECT_EQ(services.active_selection(), edited.second);
    services.clear_selection();
    EXPECT_FALSE(services.has_selection());
  }

  TEST_F(editor_services_tests, an_object_that_is_destroyed_leaves_the_selection) {
    edited_scene edited;
    editor_services& services = edited.services;
    services.set_selection(edited.first);
    /// as a window that writes the list itself does: seen on the next frame
    services.current_selection.objects.push_back(edited.under);
    const uint64_t before = services.selection_revision();
    services.tick_selection();
    EXPECT_NE(services.selection_revision(), before);

    edited.s.destroy_object(edited.under);
    services.tick_selection();
    ASSERT_EQ(services.current_selection.objects.size(), 1u);
    EXPECT_EQ(services.active_selection(), edited.first);
  }

  TEST_F(editor_services_tests, a_value_that_is_dragged_is_one_step_once_it_stops) {
    edited_scene edited;
    editor_services& services = edited.services;
    /// the first quiet frame keeps what the scene is before any edit
    services.tick_edit_tracker();
    EXPECT_FALSE(services.can_undo_scene_edit());

    for (int32_t x = 1; x <= 5; ++x) {
      env->call_static_dotnet_method<int32_t>("EditorProbe", "ChangeFirstsPlace", int32_t{ x });
      services.tick_edit_tracker();
    }
    EXPECT_EQ(services.editing_history.undo_count(), 0u);
    EXPECT_TRUE(services.has_unsaved_scene_edits());

    edited.quiet_frames();
    EXPECT_EQ(services.editing_history.undo_count(), 1u);
    EXPECT_TRUE(services.editing_history.undo_name().empty());

    EXPECT_EQ(probe("Undo"), 1);
    EXPECT_EQ(probe("FirstsPlace"), 0);
    services.redo_scene_edit();
    EXPECT_EQ(probe("FirstsPlace"), 5);
  }

  TEST_F(editor_services_tests, an_edit_block_is_one_step_at_once_with_its_name) {
    edited_scene edited;
    edited.services.tick_edit_tracker();
    EXPECT_EQ(probe("AnEditIsOneStep"), 1);
  }

  TEST_F(editor_services_tests, an_edit_ends_a_drag_that_is_still_open) {
    edited_scene edited;
    editor_services& services = edited.services;
    services.tick_edit_tracker();

    env->call_static_dotnet_method<int32_t>("EditorProbe", "ChangeFirstsPlace", int32_t{ 7 });
    services.begin_edit("Rename");
    edited.s.rename_object(edited.second, "Renamed");
    services.end_edit();

    /// two steps: the drag, then the rename
    ASSERT_EQ(services.editing_history.undo_count(), 2u);
    EXPECT_EQ(services.editing_history.undo_name(), "Rename");
    services.undo_scene_edit();
    EXPECT_NE(edited.s.find_object(std::string_view{ "Second" }), nullptr);
    EXPECT_EQ(probe("FirstsPlace"), 7);
    services.undo_scene_edit();
    EXPECT_EQ(probe("FirstsPlace"), 0);
  }

  TEST_F(editor_services_tests, nothing_is_recorded_while_the_scene_plays) {
    edited_scene edited;
    editor_services& services = edited.services;
    services.tick_edit_tracker();

    edited.s.play();
    services.begin_edit("Rename");
    edited.s.rename_object(edited.first, "Renamed");
    services.end_edit();
    services.notify_scene_edited();
    edited.quiet_frames();
    EXPECT_FALSE(services.can_undo_scene_edit());
    EXPECT_EQ(services.editing_history.undo_count(), 0u);
    edited.s.stop();
  }

  TEST_F(editor_services_tests, the_history_gives_up_its_oldest_edits_past_its_limit) {
    edited_scene edited;
    editor_services& services = edited.services;
    services.tick_edit_tracker();
    const size_t one_edit = edited.s.capture_snapshot().size() * 2;
    /// room for two edits and part of a third
    services.editing_history.set_byte_limit(one_edit * 2 + one_edit / 2);

    for (const std::string_view name : { "One", "Two", "Three", "Four" }) {
      services.begin_edit(name);
      edited.s.rename_object(edited.first, name);
      services.end_edit();
    }
    EXPECT_LE(services.editing_history.kept_bytes(), one_edit * 3);
    EXPECT_LT(services.editing_history.undo_count(), 4u);
    EXPECT_GE(services.editing_history.undo_count(), 1u);
    EXPECT_EQ(services.editing_history.undo_name(), "Four");

    /// what is left undoes as far back as it goes, and no further
    while (services.can_undo_scene_edit()) {
      services.undo_scene_edit();
    }
    EXPECT_EQ(edited.s.find_object(std::string_view{ "First" }), nullptr);

    /// however small the limit, the newest edit can be undone
    services.editing_history.set_byte_limit(1);
    services.begin_edit("Last");
    edited.s.add_object_tag(edited.s.get_children_ids(edited.s.root_object().id).front(), "tagged");
    services.end_edit();
    EXPECT_EQ(services.editing_history.undo_count(), 1u);
  }

  TEST_F(editor_services_tests, a_scenes_revision_moves_when_what_a_list_shows_changes) {
    edited_scene edited;
    scene& s = edited.s;
    uint64_t seen = s.revision();
    const auto moved = [&s, &seen]() -> bool {
      const bool changed = s.revision() != seen;
      seen = s.revision();
      return changed;
    };
    EXPECT_NE(seen, 0u);

    const natural_t made = s.create_object("Made").id;
    EXPECT_TRUE(moved());
    s.rename_object(made, "Named");
    EXPECT_TRUE(moved());
    EXPECT_TRUE(s.reparent_object(made, edited.first));
    EXPECT_TRUE(moved());
    s.set_object_visible(made, false);
    EXPECT_TRUE(moved());
    s.add_object_tag(made, "tag");
    EXPECT_TRUE(moved());
    s.remove_object_tag(made, "tag");
    EXPECT_TRUE(moved());
    s.destroy_object(made);
    EXPECT_TRUE(moved());

    /// and not for what a list does not show, or for a change that changes nothing
    s.set_transform(edited.first, transform(glm::vec3(1.f, 2.f, 3.f)));
    s.set_object_visible(edited.first, true);
    EXPECT_FALSE(s.reparent_object(edited.first, edited.first));
    EXPECT_FALSE(moved());

    /// no two scenes hold the same number: another scene is another revision
    scene another("Another");
    EXPECT_NE(another.revision(), s.revision());
    EXPECT_EQ(bindings::native_scene_revision(), s.revision());
  }

  TEST_F(editor_services_tests, a_script_selects_through_the_editors_api) {
    edited_scene edited;
    EXPECT_EQ(probe("SelectionRoundTrip"), 1);

    EXPECT_EQ(probe("SelectSecond"), 1);
    EXPECT_EQ(edited.services.active_selection(), edited.second);
    /// what the editor itself selects, a script sees
    edited.services.add_to_selection(edited.first);
    EXPECT_EQ(probe("SelectedCount"), 2);
  }

  TEST_F(editor_services_tests, a_kept_view_of_the_scene_is_read_again_only_when_it_changes) {
    edited_scene edited;
    EXPECT_EQ(probe("SceneViewFollows"), 1);
  }

  TEST_F(editor_services_tests, component_types_are_listed_added_and_removed_by_id) {
    edited_scene edited;
    EXPECT_EQ(abi::oe_component_type_count(), 14);

    uint64_t id = 0;
    nbool32 removable = true;
    EXPECT_STREQ(abi::oe_component_type_at(0, &id, &removable), "Transform");
    EXPECT_NE(id, 0u);
    EXPECT_FALSE(removable);
    /// an index that is none is no type
    EXPECT_STREQ(abi::oe_component_type_at(99, &id, &removable), "");
    EXPECT_EQ(id, 0u);
    EXPECT_STREQ(abi::oe_component_type_at(-1, &id, &removable), "");

    EXPECT_EQ(probe("ComponentTypes"), 1);
  }

  TEST_F(editor_services_tests, playback_and_saving_are_asked_of_the_driver) {
    edited_scene edited;
    scene& s = edited.s;
    /// with no driver to ask, asking does nothing
    bindings::native_editor_playback_command(0);
    EXPECT_EQ(bindings::native_editor_playback_state(), 0);
    EXPECT_EQ(probe("SaveScene"), 0);

    edited.services.send_playback_command = [&s](const std::string_view command) {
      if (command == "play") {
        s.play();
      } else if (command == "pause") {
        s.pause();
      } else {
        s.stop();
      }
    };
    EXPECT_EQ(probe("PlaybackFollows"), 1);
    /// a command that is none is not sent
    bindings::native_editor_playback_command(7);
    EXPECT_EQ(bindings::native_editor_playback_state(), 0);

    /// held and played again, stop still puts back the scene from before the first play
    bindings::native_editor_playback_command(0);
    env->call_static_dotnet_method<int32_t>("EditorProbe", "ChangeFirstsPlace", int32_t{ 3 });
    bindings::native_editor_playback_command(1);
    EXPECT_EQ(bindings::native_editor_playback_state(), 2);
    bindings::native_editor_playback_command(0);
    EXPECT_EQ(probe("FirstsPlace"), 3);
    bindings::native_editor_playback_command(2);
    EXPECT_EQ(probe("FirstsPlace"), 0);

    uint32_t saves = 0;
    edited.services.save_scene = [&saves]() -> bool {
      ++saves;
      return true;
    };
    EXPECT_EQ(probe("SaveScene"), 1);
    EXPECT_EQ(saves, 1u);
  }

  TEST_F(editor_services_tests, commands_run_by_name_and_go_with_their_assembly) {
    edited_scene edited;
    /// nothing is called each frame until there is a command
    EXPECT_FALSE(bindings::run_editor_frame_function());
    EXPECT_EQ(probe("CommandsRun"), 1);
    EXPECT_EQ(probe("CommandCount"), 2);
    EXPECT_TRUE(bindings::run_editor_frame_function());

    /// the two that are left were written in the testing assembly
    env->unload_dotnet_module(env->get_dotnet_module_by_asset_path(testing_dll));
    EXPECT_TRUE(bindings::run_editor_frame_function());
    ASSERT_NE(env->load_dotnet_module(testing_dll.string()), nullptr);
    EXPECT_EQ(probe("CommandCount"), 0);

    /// and when the editor's own assembly goes, it leaves nothing to call into
    env->unload_dotnet_module(api);
    api = nullptr;
    EXPECT_FALSE(bindings::run_editor_frame_function());
  }

  TEST_F(editor_services_tests, the_projects_scenes_are_listed_and_one_is_asked_for) {
    edited_scene edited;
    editor_services& services = edited.services;
    /// with no driver to list them there are none
    EXPECT_EQ(probe("ProjectScenesCount"), 0);

    uint32_t asked = 0;
    services.list_scenes = [&asked](ostd::vector<editor_services::scene_listing>& listed) {
      ++asked;
      listed.push_back({ .id = 7, .name = "Menu", .scene_name = "menu", .active = false, .starting = true });
      listed.push_back({ .id = 9, .name = "Level", .scene_name = "Level", .active = true, .starting = false });
    };
    /// the list is kept until the scene in the editor changes
    EXPECT_EQ(probe("ProjectScenesCount"), 0);
    EXPECT_EQ(asked, 0u);
    edited.s.rename_object(edited.first, "Changed");

    EXPECT_FALSE(services.pending_scene_activation.has_value());
    EXPECT_EQ(probe("ProjectScenesListed"), 7);
    EXPECT_EQ(asked, 1u);
    ASSERT_TRUE(services.pending_scene_activation.has_value());
    EXPECT_EQ(*services.pending_scene_activation, 7u);

    /// an index that is none is no scene
    natural_t id = 1;
    nbool32 active = true;
    nbool32 starting = true;
    EXPECT_STREQ(bindings::native_editor_scenes_at(5, &id, &active, &starting), "");
    EXPECT_EQ(id, 0u);
    EXPECT_FALSE(active);
    EXPECT_STREQ(bindings::native_editor_scenes_scene_name(-1), "");
  }

  TEST_F(editor_services_tests, an_objects_place_in_the_tree_is_the_same_after_an_undo) {
    edited_scene edited;
    edited.services.tick_edit_tracker();
    EXPECT_EQ(probe("PathKeysOutliveAnUndo"), 1);
  }

  TEST_F(editor_services_tests, a_compile_has_nothing_to_work_on_with_no_project) {
    edited_scene edited;
    EXPECT_EQ(probe("CompileHasNoProject"), 1);
    uint64_t stamp = 0;
    EXPECT_EQ(bindings::native_editor_compile_poll(&stamp), static_cast<int32_t>(compile_service::state::NO_PROJECT));
    EXPECT_NE(stamp, 0u);
    EXPECT_EQ(bindings::native_editor_compile_result(), static_cast<int32_t>(compile_service::result::NONE));
    EXPECT_EQ(bindings::native_editor_compile_count(static_cast<int32_t>(bindings::compile_text::SCENES)), 0);
    EXPECT_STREQ(bindings::native_editor_compile_text(static_cast<int32_t>(bindings::compile_text::SCENES), 3), "");
    EXPECT_STREQ(bindings::native_editor_compile_text(900, 0), "");
  }

  TEST_F(editor_services_tests, a_picked_folder_is_handed_over_on_a_later_frame_and_once) {
    edited_scene edited;
    editor_services& services = edited.services;
    /// with nothing to show a dialog with, asking says so
    EXPECT_EQ(probe("AskForFolder"), 0);

    uint32_t shown = 0;
    services.show_folder_dialog = [&shown]() {
      ++shown;
    };
    EXPECT_EQ(probe("AskForFolder"), 1);
    EXPECT_EQ(shown, 1u);
    /// nothing was picked yet: a frame hands nothing over
    EXPECT_TRUE(bindings::run_editor_frame_function());
    EXPECT_EQ(env->call_static_dotnet_method<int32_t>("EditorProbe", "PickedIs", int32_t{ 0 }), 1);

    /// the dialog answers on its own thread; C# hears of it on the main one, a frame later
    services.folder_was_picked("C:/games/caf\xc3\xa9");
    EXPECT_EQ(env->call_static_dotnet_method<int32_t>("EditorProbe", "PickedIs", int32_t{ 0 }), 1);
    EXPECT_TRUE(bindings::run_editor_frame_function());
    EXPECT_EQ(env->call_static_dotnet_method<int32_t>("EditorProbe", "PickedIs", int32_t{ 1 }), 1);
    EXPECT_FALSE(services.take_picked_folder().has_value());

    /// a dialog that was cancelled answers nothing, and nothing is handed over
    EXPECT_EQ(probe("AskForFolder"), 1);
    EXPECT_TRUE(bindings::run_editor_frame_function());
    EXPECT_EQ(env->call_static_dotnet_method<int32_t>("EditorProbe", "PickedIs", int32_t{ 0 }), 1);
  }

  TEST_F(editor_services_tests, a_project_is_made_as_the_create_tool_makes_one) {
    edited_scene edited;
    std::error_code ec;
    const filepath folder = std::filesystem::temp_directory_path() / "other-editor-services-tests";
    std::filesystem::remove_all(folder, ec);
    std::filesystem::create_directories(folder, ec);
    ASSERT_FALSE(ec) << ec.message();

    EXPECT_EQ(probe("CreateProject"), 1);
    const filepath made = folder / "probe-game";
    EXPECT_TRUE(std::filesystem::exists(made / "probe-game.toml"));
    EXPECT_TRUE(std::filesystem::exists(made / "probe-game.lua"));
    EXPECT_TRUE(std::filesystem::exists(made / "assets" / "scenes" / "main.oscn"));
    EXPECT_TRUE(std::filesystem::is_directory(made / "src"));
    /// the same file, which is not always the same text: C# names the temporary folder
    ///  in full where this names it as the environment does, and on a CI runner that is
    ///  its short form (RUNNER~1 for runneradmin)
    EXPECT_TRUE(std::filesystem::equivalent(filepath{ edited.services.created_project }, made / "probe-game.toml", ec)) << edited.services.created_project;

    /// what was typed is in the project file, and the suite's environment wired the scripts
    std::ifstream file(made / "probe-game.toml", std::ios::binary);
    const std::string text{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
    /// its paths are written from the folder that holds it, which is where they are read from
    EXPECT_TRUE(text.contains("path = \"probe-game/assets/scenes/main.oscn\""));
    EXPECT_TRUE(text.contains("cs_project = \"probe-game/ProbeGame.csproj\""));
    EXPECT_TRUE(text.contains("projectrc-path = \"probe-game/probe-game.lua\""));
    EXPECT_TRUE(text.contains("Someone"));
    EXPECT_TRUE(text.contains("made by the suite"));
    EXPECT_TRUE(std::filesystem::exists(made / "ProbeGame.csproj"));

    /// a project that could not be made leaves nothing behind
    EXPECT_FALSE(std::filesystem::exists(folder / "9 lives"));
    file.close();
    std::filesystem::remove_all(folder, ec);
  }

  TEST_F(editor_services_tests, with_no_editor_the_api_answers_nothing) {
    {
      edited_scene edited;
      EXPECT_EQ(probe("SelectSecond"), 1);
    }
    /// the services are gone: nothing is selected, nothing can be undone, nothing breaks
    EXPECT_EQ(bindings::get_editor_services(), nullptr);
    EXPECT_EQ(bindings::native_editor_selection_active(), 0u);
    EXPECT_EQ(bindings::native_editor_selection_revision(), 0u);
    EXPECT_FALSE(bindings::native_editor_edits_can_undo());
    EXPECT_NO_FATAL_FAILURE(bindings::native_editor_edits_begin("Nothing"));
    EXPECT_NO_FATAL_FAILURE(bindings::native_editor_edits_end());
    EXPECT_NO_FATAL_FAILURE(bindings::native_editor_selection_set(3));
    EXPECT_EQ(bindings::native_editor_playback_state(), 0);
    EXPECT_NO_FATAL_FAILURE(bindings::native_editor_playback_command(0));
    EXPECT_FALSE(bindings::native_editor_scene_save());
    EXPECT_EQ(bindings::native_editor_scenes_read(), 0);
    EXPECT_NO_FATAL_FAILURE(bindings::native_editor_scenes_activate(3));
    uint64_t stamp = 7;
    EXPECT_EQ(bindings::native_editor_compile_poll(&stamp), 0);
    EXPECT_EQ(stamp, 0u);
    EXPECT_FALSE(bindings::native_editor_pick_folder());
    EXPECT_STRNE(bindings::native_editor_create_project("a-game", "somewhere", "", ""), "");
  }

}  // namespace other
