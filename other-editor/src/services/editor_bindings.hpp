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
    /// ends the action that is open and records nothing
    void native_editor_edits_drop();
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

    /// where the camera of the editor's view is, which way it looks and the point it turns
    ///  round, three floats each. false, and nothing written, when there is no view
    nbool32 native_editor_view_camera(float* position, float* direction, float* focus_point);
    /// brings the selection to the middle of the view. false with nothing selected
    nbool32 native_editor_view_focus_selection();
    /// turns the view's camera round the point it turns round, by degrees to the right and down
    void native_editor_view_orbit(float right, float down);
    /// turns it round that point until it looks the given way
    void native_editor_view_look_along(float x, float y, float z);
    /// where the view's picture is on the screen and its size, four floats, and what is
    ///  true of it: bit 0 it is on the screen, bit 1 the pointer is over it, bit 2 it was
    ///  clicked this frame
    uint32_t native_editor_view_state(float* rectangle);
    /// the ray from the view's camera through a place of the screen, three floats each.
    ///  false when there is no view
    nbool32 native_editor_view_ray(float x, float y, float* origin, float* direction);
    /// where a point of the world is on the screen, two floats. false when it is behind
    ///  the camera, or there is no view
    nbool32 native_editor_view_place(float x, float y, float z, float* place);
    /// how much of the world one unit of the screen covers at that point's distance
    float native_editor_view_pixel_size(float x, float y, float z);
    /// the id of the object drawn at a place of the screen. 0 for none
    uint64_t native_editor_view_pick(float x, float y);
    /// whether the ground grid is drawn in the view. shown is 1 to show it, 0 to hide it,
    ///  and anything else to only ask
    nbool32 native_editor_view_grid(int32_t shown);
    /// makes an object with the model at that path where the ray from a place of the
    ///  screen meets the scene, selects it, and gives its id. 0 when it could not
    uint64_t native_editor_view_place_model(const char* path, float x, float y);

    /// the handles of the view's tools. each is asked for every frame its tool runs, by an
    ///  id of its own, and answers with handle_flags; vectors are three floats. they
    ///  answer 0 and draw nothing when the view's tools are not being run
    /// bit 0: some handle is hot. bit 1: one is being dragged
    uint32_t native_editor_handles_state();
    /// what a drag goes in steps of while it snaps: metres, degrees, times the size
    void native_editor_handles_snaps(float* move, float* turn_degrees, float* scale);
    uint32_t native_editor_handles_slide_axis(uint64_t id, float* position, float* axis, float reach, float snap);
    uint32_t native_editor_handles_slide_plane(uint64_t id, float* position, float* u, float* v, float near_edge, float far_edge, float snap);
    uint32_t native_editor_handles_turn(uint64_t id, float* centre, float* axis, float radius, float* angle, float snap, uint32_t near_half_only);
    /// from the centre of the ring being turned towards where it was taken hold of, three
    ///  floats. false, and nothing written, when no handle is being dragged
    nbool32 native_editor_handles_grab_direction(float* direction);
    uint32_t native_editor_handles_stretch_axis(uint64_t id, float* centre, float* axis, float reach, float* factor, float snap);
    uint32_t native_editor_handles_stretch_all(uint64_t id, float* centre, float radius_pixels, float* factor, float snap);
    /// shapes of the world drawn over the view's picture, in front of everything in it.
    ///  color is as ImGui packs one; a thickness and a dot's radius are in the UI's units
    void native_editor_handles_line(float* a, float* b, uint32_t color, float thickness);
    void native_editor_handles_arrow(float* from, float* to, uint32_t color, float thickness);
    void native_editor_handles_circle(float* centre, float* axis, float radius, uint32_t color, float thickness);
    /// part of a circle round an axis: from the direction `start`, which is square to the
    ///  axis, so many radians counter-clockwise seen from where the axis points. the pie is
    ///  that part filled in, to the centre
    void native_editor_handles_arc(float* centre, float* axis, float* start, float radians, float radius, uint32_t color, float thickness);
    void native_editor_handles_pie(float* centre, float* axis, float* start, float radians, float radius, uint32_t color);
    void native_editor_handles_quad(float* a, float* b, float* c, float* d, uint32_t color);
    void native_editor_handles_dot(float* at, float radius_pixels, uint32_t color);
    void native_editor_handles_label(float* at, const char* text, uint32_t color);

    /// fn is a void (*)(), called while the view's window is drawn, after its picture:
    ///  where the C# side runs its scene tools. null for none
    void native_editor_set_scene_view_function(void* fn);
    /// called by the view's window. false when there is nothing to call
    bool run_scene_view_function();

    /// reads the project's scenes again and says how many there are. at and scene_name
    ///  then answer from what was read: at gives the name the project has for the scene
    int32_t native_editor_scenes_read();
    const char* native_editor_scenes_at(int32_t index, natural_t* out_id, nbool32* out_active, nbool32* out_starting);
    const char* native_editor_scenes_scene_name(int32_t index);
    /// asks for that scene to be the active one; it is, after the frame
    void native_editor_scenes_activate(natural_t scene_id);

    /// what EditorCompileText and EditorCompileCount are asked for: the first five and the
    ///  two sizes are one text each, the rest are lists.
    ///  \note CompileText in C# is the same list in the same order
    enum class compile_text : int32_t {
      PROJECT_FILE,
      PUBLIC_NAME,
      OUTPUT_FOLDER,
      STARTING_SCENE,
      MAIN_PIPELINE,
      SCENES,
      PIPELINES,
      VM_SOURCES,
      HOOKS,
      PROBLEMS,
      LEFT_OUT,
      LEFT_OUT_SIZE,
      REPORT_PACKAGE,
      REPORT_ERRORS,
      REPORT_WARNINGS,
      REPORT_LEFT_OUT,
      REPORT_LEFT_OUT_SIZE,
    };

    /// once a frame by the window that shows a compile: a compile_service::state, and the
    ///  stamp that moves when any text below is not what it was
    int32_t native_editor_compile_poll(uint64_t* out_stamp);
    /// reads the open project again
    void native_editor_compile_refresh();
    int32_t native_editor_compile_count(int32_t what);
    const char* native_editor_compile_text(int32_t what, int32_t index);
    /// why a public name cannot be used; empty when it can
    const char* native_editor_compile_name_problem(const char* public_name);
    /// empty when the compile started, otherwise why it did not
    const char* native_editor_compile_start(const char* public_name, const char* output_folder, const char* starting_scene, const char* main_pipeline, nbool32 debug);
    /// true while a compile runs: how many of its stages are done, and whether it was told to stop
    nbool32 native_editor_compile_progress(int32_t* out_stage, int32_t* out_stages, nbool32* out_cancelling);
    const char* native_editor_compile_stage_label();
    void native_editor_compile_cancel();
    /// a compile_service::result: what came of the last compile
    int32_t native_editor_compile_result();

    /// the system's folder dialog. the folder is taken on a later frame, once: false until
    ///  one was picked
    nbool32 native_editor_pick_folder();
    nbool32 native_editor_take_picked_folder(char* out_path, int32_t capacity);

    /// makes a project as `oecli create` does, in a folder of its name inside that folder.
    ///  empty when it was made, otherwise why it was not. the editor opens it when no
    ///  project is open and it was made in the folder the editor works from; opening says
    ///  whether it was told to
    const char* native_editor_create_project(const char* name, const char* folder, const char* author, const char* description);
    const char* native_editor_created_project();
    nbool32 native_editor_created_project_opening();

    /// fn is a void (*)(), called once a frame while the UI is drawn: where the C# side
    ///  checks its commands' keys. null for none
    void native_editor_set_frame_function(void* fn);
    /// called by the editor inside the UI's frame. false when there is nothing to call
    bool run_editor_frame_function();

  }  // namespace bindings
}  // namespace other

#endif  // OTHER_EDITOR_SERVICES_EDITOR_BINDINGS_HPP
