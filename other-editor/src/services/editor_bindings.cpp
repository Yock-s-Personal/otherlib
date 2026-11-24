/**
 * \file services/editor_bindings.cpp
 **/
#include "services/editor_bindings.hpp"

#include <iterator>
#include <string_view>

#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "dotnet/dotnet_host.hpp"

#include "scripting/dotnet_bindings/project_bindings.hpp"
#include "scripting/main_thread_only.hpp"

#include "services/editor_services.hpp"

namespace other {
  namespace bindings {
    namespace {

      editor_services* editor = nullptr;
      /// code of OtherEditor.dll: set by it, and taken back by it before it is unloaded
      void (*frame_function)() = nullptr;

      template <auto Fn>
      void bind(dotnet_host& host, const std::string_view name, size_t& refused) {
        if (bind_to_script<Fn>(host, name) != binding_result::BOUND) {
          ++refused;
        }
      }

    }  // namespace

    void set_editor_services(editor_services* services) {
      editor = services;
    }

    editor_services* get_editor_services() {
      return editor;
    }

    size_t bind_editor_functions(dotnet_host& host) {
      PROFILE_SECTION("bindings::bind_editor_functions");
      host.rediscover_binding_points();
      /// an assembly that was just loaded has given no function yet
      frame_function = nullptr;

      size_t refused = 0;
      /// Selection.
      bind<native_editor_selection_revision>(host, "EditorSelectionRevision", refused);
      bind<native_editor_selection_active>(host, "EditorSelectionActive", refused);
      bind<native_editor_selection_get>(host, "EditorSelectionGet", refused);
      bind<native_editor_selection_set>(host, "EditorSelectionSet", refused);
      bind<native_editor_selection_add>(host, "EditorSelectionAdd", refused);
      bind<native_editor_selection_remove>(host, "EditorSelectionRemove", refused);
      bind<native_editor_selection_clear>(host, "EditorSelectionClear", refused);
      /// Edits.
      bind<native_editor_edits_changed>(host, "EditorEditsChanged", refused);
      bind<native_editor_edits_begin>(host, "EditorEditsBegin", refused);
      bind<native_editor_edits_end>(host, "EditorEditsEnd", refused);
      bind<native_editor_edits_can_undo>(host, "EditorEditsCanUndo", refused);
      bind<native_editor_edits_can_redo>(host, "EditorEditsCanRedo", refused);
      bind<native_editor_edits_undo>(host, "EditorEditsUndo", refused);
      bind<native_editor_edits_redo>(host, "EditorEditsRedo", refused);
      bind<native_editor_edits_undo_name>(host, "EditorEditsUndoName", refused);
      bind<native_editor_edits_has_unsaved>(host, "EditorEditsHasUnsaved", refused);
      /// Playback, EditorScene.
      bind<native_editor_playback_state>(host, "EditorPlaybackState", refused);
      bind<native_editor_playback_command>(host, "EditorPlaybackCommand", refused);
      bind<native_editor_scene_save>(host, "EditorSceneSave", refused);
      /// SceneViewport.
      bind<native_editor_view_camera>(host, "EditorViewCamera", refused);
      bind<native_editor_view_focus_selection>(host, "EditorViewFocusSelection", refused);
      /// ProjectScenes.
      bind<native_editor_scenes_read>(host, "EditorScenesRead", refused);
      bind<native_editor_scenes_at>(host, "EditorScenesAt", refused);
      bind<native_editor_scenes_scene_name>(host, "EditorScenesSceneName", refused);
      bind<native_editor_scenes_activate>(host, "EditorScenesActivate", refused);
      /// Commands.
      bind<native_editor_set_frame_function>(host, "EditorSetFrameFunction", refused);
      /// Compile.
      bind<native_editor_compile_poll>(host, "EditorCompilePoll", refused);
      bind<native_editor_compile_refresh>(host, "EditorCompileRefresh", refused);
      bind<native_editor_compile_count>(host, "EditorCompileCount", refused);
      bind<native_editor_compile_text>(host, "EditorCompileText", refused);
      bind<native_editor_compile_name_problem>(host, "EditorCompileNameProblem", refused);
      bind<native_editor_compile_start>(host, "EditorCompileStart", refused);
      bind<native_editor_compile_progress>(host, "EditorCompileProgress", refused);
      bind<native_editor_compile_stage_label>(host, "EditorCompileStageLabel", refused);
      bind<native_editor_compile_cancel>(host, "EditorCompileCancel", refused);
      bind<native_editor_compile_result>(host, "EditorCompileResult", refused);
      /// Dialogs, Projects.
      bind<native_editor_pick_folder>(host, "EditorPickFolder", refused);
      bind<native_editor_take_picked_folder>(host, "EditorTakePickedFolder", refused);
      bind<native_editor_create_project>(host, "EditorCreateProject", refused);
      bind<native_editor_created_project>(host, "EditorCreatedProject", refused);
      bind<native_editor_created_project_opening>(host, "EditorCreatedProjectOpening", refused);
      /// ProjectScripts, ProjectFile: what the engine keeps about the open project that only
      ///  an editor shows. the engine has the functions and binds none of them itself
      bind<native_project_has_file_notice>(host, "EditorProjectHasFileNotice", refused);
      bind<native_project_get_file_notice_error>(host, "EditorProjectGetFileNoticeError", refused);
      bind<native_project_get_file_notice_part_count>(host, "EditorProjectGetFileNoticePartCount", refused);
      bind<native_project_get_file_notice_part>(host, "EditorProjectGetFileNoticePart", refused);
      bind<native_project_get_script_project_path>(host, "EditorProjectGetScriptProjectPath", refused);
      bind<native_project_scripts_building>(host, "EditorProjectScriptsBuilding", refused);
      bind<native_project_scripts_loaded>(host, "EditorProjectScriptsLoaded", refused);
      bind<native_project_has_script_failure>(host, "EditorProjectHasScriptFailure", refused);
      bind<native_project_get_script_failure_project_path>(host, "EditorProjectGetScriptFailureProjectPath", refused);
      bind<native_project_get_script_failure_reason>(host, "EditorProjectGetScriptFailureReason", refused);
      bind<native_project_get_script_failure_details>(host, "EditorProjectGetScriptFailureDetails", refused);
      bind<native_project_script_failure_can_retry>(host, "EditorProjectScriptFailureCanRetry", refused);
      bind<native_project_request_script_rebuild>(host, "EditorProjectRequestScriptRebuild", refused);
      return refused;
    }

    uint64_t native_editor_selection_revision() {
      return editor != nullptr ? editor->selection_revision() : 0;
    }

    natural_t native_editor_selection_active() {
      return editor != nullptr ? editor->active_selection() : 0;
    }

    void native_editor_selection_get(natural_t* out_ids, int32_t* out_count, int32_t max_count) {
      *out_count = 0;
      if (editor == nullptr || editor->current_selection.scene_ptr == nullptr) {
        return;
      }
      for (const natural_t object_id : editor->current_selection.objects) {
        if (*out_count >= max_count) {
          break;
        }
        out_ids[(*out_count)++] = object_id;
      }
    }

    void native_editor_selection_set(natural_t object_id) {
      if (editor != nullptr) {
        editor->set_selection(object_id);
      }
    }

    void native_editor_selection_add(natural_t object_id) {
      if (editor != nullptr) {
        editor->add_to_selection(object_id);
      }
    }

    void native_editor_selection_remove(natural_t object_id) {
      if (editor != nullptr) {
        editor->remove_from_selection(object_id);
      }
    }

    void native_editor_selection_clear() {
      if (editor != nullptr) {
        editor->clear_selection();
      }
    }

    void native_editor_edits_changed() {
      if (editor != nullptr) {
        editor->notify_scene_edited();
      }
    }

    void native_editor_edits_begin(const char* name) {
      if (editor != nullptr) {
        editor->begin_edit(name != nullptr ? std::string_view{ name } : std::string_view{});
      }
    }

    void native_editor_edits_end() {
      if (editor != nullptr) {
        editor->end_edit();
      }
    }

    nbool32 native_editor_edits_can_undo() {
      return editor != nullptr && editor->can_undo_scene_edit();
    }

    nbool32 native_editor_edits_can_redo() {
      return editor != nullptr && editor->can_redo_scene_edit();
    }

    void native_editor_edits_undo() {
      if (editor != nullptr) {
        editor->undo_scene_edit();
      }
    }

    void native_editor_edits_redo() {
      if (editor != nullptr) {
        editor->redo_scene_edit();
      }
    }

    const char* native_editor_edits_undo_name() {
      return returned_string(editor != nullptr ? editor->editing_history.undo_name() : std::string_view{});
    }

    nbool32 native_editor_edits_has_unsaved() {
      return editor != nullptr && editor->has_unsaved_scene_edits();
    }

    int32_t native_editor_playback_state() {
      const scene* active = editor != nullptr ? editor->current_selection.scene_ptr : nullptr;
      if (active == nullptr) {
        return 0;
      }
      if (active->is_playing()) {
        return 1;
      }
      return active->is_paused() ? 2 : 0;
    }

    void native_editor_playback_command(int32_t command) {
      constexpr std::string_view commands[] = { "play", "pause", "stop" };
      if (editor == nullptr || !editor->send_playback_command || command < 0 || command >= static_cast<int32_t>(std::size(commands))) {
        return;
      }
      editor->send_playback_command(commands[command]);
    }

    nbool32 native_editor_scene_save() {
      return editor != nullptr && editor->save_scene && editor->save_scene();
    }

    nbool32 native_editor_view_camera(float* position, float* direction, float* focus_point) {
      if (editor == nullptr || editor->view_camera == nullptr || position == nullptr || direction == nullptr || focus_point == nullptr) {
        return false;
      }
      const camera& cam = *editor->view_camera;
      const glm::vec3 focus = editor->camera_controller.focus_point(cam);
      for (int32_t i = 0; i < 3; ++i) {
        position[i] = cam.position[i];
        direction[i] = cam.direction[i];
        focus_point[i] = focus[i];
      }
      return true;
    }

    nbool32 native_editor_view_focus_selection() {
      return editor != nullptr && editor->focus_view_on_selection();
    }

    int32_t native_editor_scenes_read() {
      if (editor == nullptr) {
        return 0;
      }
      editor->listed_scenes.clear();
      if (editor->list_scenes) {
        editor->list_scenes(editor->listed_scenes);
      }
      return static_cast<int32_t>(editor->listed_scenes.size());
    }

    const char* native_editor_scenes_at(int32_t index, natural_t* out_id, nbool32* out_active, nbool32* out_starting) {
      const editor_services::scene_listing* listed = nullptr;
      if (editor != nullptr && index >= 0 && static_cast<size_t>(index) < editor->listed_scenes.size()) {
        listed = &editor->listed_scenes[index];
      }
      *out_id = listed != nullptr ? listed->id : 0;
      *out_active = listed != nullptr && listed->active;
      *out_starting = listed != nullptr && listed->starting;
      return returned_string(listed != nullptr ? std::string_view{ listed->name } : std::string_view{});
    }

    const char* native_editor_scenes_scene_name(int32_t index) {
      if (editor == nullptr || index < 0 || static_cast<size_t>(index) >= editor->listed_scenes.size()) {
        return returned_string({});
      }
      return returned_string(editor->listed_scenes[index].scene_name);
    }

    void native_editor_scenes_activate(natural_t scene_id) {
      if (editor != nullptr && scene_id != 0) {
        editor->pending_scene_activation = scene_id;
      }
    }

    void native_editor_set_frame_function(void* fn) {
      frame_function = reinterpret_cast<void (*)()>(fn);
    }

    bool run_editor_frame_function() {
      if (frame_function == nullptr) {
        return false;
      }
      frame_function();
      return true;
    }

  }  // namespace bindings
}  // namespace other
