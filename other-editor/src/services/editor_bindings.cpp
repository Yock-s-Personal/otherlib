/**
 * \file services/editor_bindings.cpp
 **/
#include "services/editor_bindings.hpp"

#include <iterator>
#include <string_view>

#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "dotnet/dotnet_host.hpp"

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
      /// Commands.
      bind<native_editor_set_frame_function>(host, "EditorSetFrameFunction", refused);
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
