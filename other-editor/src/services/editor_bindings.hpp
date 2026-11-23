/**
 * \file services/editor_bindings.hpp
 * what the editor's C# API (Other.Editor, in OtherEditor.dll) asks the editor. these are
 * the editor's own bindings: they are bound when that assembly has been loaded, by a
 * driver that is an editor, and are not in the library a game links
 **/
#ifndef OTHER_EDITOR_SERVICES_EDITOR_BINDINGS_HPP
#define OTHER_EDITOR_SERVICES_EDITOR_BINDINGS_HPP

#include "core/defines.hpp"

#include "dotnet/types.hpp"

namespace other {

  class dotnet_host;
  struct editor_services;

  namespace bindings {

    /// what the bindings below ask. null while there is no editor: each then answers
    ///  "nothing" and changes nothing
    void set_editor_services(editor_services* services);
    editor_services* get_editor_services();

    /// binds every function below to its field in OtherEditor.dll, which has to be loaded.
    ///  the number that could not be bound; each is said in the log with how its two
    ///  declarations differ
    size_t bind_editor_functions(dotnet_host& host);

    uint64_t native_editor_selection_revision();
    natural_t native_editor_selection_active();
    void native_editor_selection_get(natural_t* out_ids, int32_t* out_count, int32_t max_count);
    void native_editor_selection_set(natural_t object_id);
    void native_editor_selection_add(natural_t object_id);
    void native_editor_selection_remove(natural_t object_id);
    void native_editor_selection_clear();

    void native_editor_edits_changed();
    void native_editor_edits_begin(const char* name);
    void native_editor_edits_end();
    nbool32 native_editor_edits_can_undo();
    nbool32 native_editor_edits_can_redo();
    void native_editor_edits_undo();
    void native_editor_edits_redo();
    const char* native_editor_edits_undo_name();
    nbool32 native_editor_edits_has_unsaved();

    /// 0 stopped, 1 playing, 2 paused
    int32_t native_editor_playback_state();
    /// 0 play, 1 pause, 2 stop
    void native_editor_playback_command(int32_t command);
    nbool32 native_editor_scene_save();

    /// reads the project's scenes again and says how many there are. at and scene_name
    ///  then answer from what was read: at gives the name the project has for the scene
    int32_t native_editor_scenes_read();
    const char* native_editor_scenes_at(int32_t index, natural_t* out_id, nbool32* out_active, nbool32* out_starting);
    const char* native_editor_scenes_scene_name(int32_t index);
    /// asks for that scene to be the active one; it is, after the frame
    void native_editor_scenes_activate(natural_t scene_id);

    /// fn is a void (*)(), called once a frame while the UI is drawn: where the C# side
    ///  checks its commands' keys. null for none
    void native_editor_set_frame_function(void* fn);
    /// called by the editor inside the UI's frame. false when there is nothing to call
    bool run_editor_frame_function();

  }  // namespace bindings
}  // namespace other

#endif  // OTHER_EDITOR_SERVICES_EDITOR_BINDINGS_HPP
