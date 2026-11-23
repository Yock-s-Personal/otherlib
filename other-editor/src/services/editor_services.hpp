/**
 * \file services/editor_services.hpp
 * what an editor window works with that is not the scene itself: what is selected, and the
 * edits that can be undone. it is the editor's own, apart from the library a game links, so
 * that none of it is in a packaged game; the editor's C# API (Other.Editor) reaches it
 * through the editor's bindings
 **/
#ifndef OTHER_EDITOR_SERVICES_EDITOR_SERVICES_HPP
#define OTHER_EDITOR_SERVICES_EDITOR_SERVICES_HPP

#include <functional>
#include <string>
#include <string_view>

#include "math/bounding_box.hpp"

#include "scene/scene.hpp"

#include "services/edit_stack.hpp"
#include "services/editor_selection.hpp"

namespace other {

  struct editor_services {
    selection current_selection;

    void select_object(natural_t object_id);
    void deselect_object(natural_t object_id);

    bool has_selection() const;
    bool multi_select_enabled() const;
    bounding_box get_selection_bounding_box() const;

    /// what the selection is made, by a window or a script. these say at once that it
    ///  changed; what writes current_selection itself is noticed by tick_selection
    void set_selection(natural_t object_id);
    void add_to_selection(natural_t object_id);
    void remove_from_selection(natural_t object_id);
    void clear_selection();
    /// the object the object editor shows: the last one selected. 0 for none
    natural_t active_selection() const;
    /// goes up whenever what is selected changes
    uint64_t selection_revision() const { return selection_changes; }
    /// once per frame: an object that is gone leaves the selection, and a change made by
    ///  writing current_selection is counted
    void tick_selection();

    /// stop's snapshot restore reassigns every runtime id, so the selection is carried
    ///  across a play/stop cycle by name (same identity rule as undo/redo restores)
    ostd::vector<std::string> playback_selection_names = {};

    void capture_playback_selection();
    void restore_playback_selection();

    edit_stack editing_history;

    /// snapshot-based edit coalescing: continuous widget edits open a session on first notify,
    ///  committing one {baseline, after} pair once quiet; one action is a begin_edit/end_edit pair
    struct scene_edit_tracker {
      constexpr static uint32_t kIdleFramesToCommit = 12;

      ostd::vector<uint8_t> baseline = {};
      bool session_active = false;
      uint32_t idle_frames = 0;

      /// begin_edit calls that have not ended, and the name the outermost was given
      uint32_t open_edits = 0;
      std::string open_edit_name = "";
    } edit_tracker;

    /// a scene-mutating widget changed this frame (no-op while the scene plays)
    void notify_scene_edited();
    /// once per frame: commits the open edit session after the widgets go quiet
    void tick_edit_tracker();
    /// re-baselines and clears history (scene switched)
    void reset_scene_edit_tracking();

    /// one action that is one undo step, recorded when it ends and not once the widgets go
    ///  quiet: what is changed between the two calls. they nest, and only the outermost
    ///  records. nothing is recorded while the scene plays, or when nothing changed
    void begin_edit(const std::string_view name);
    void end_edit();

    void undo_scene_edit();
    void redo_scene_edit();
    bool can_undo_scene_edit() const;
    bool can_redo_scene_edit() const;

    /// the history's change count when the active scene was last written to disk
    uint64_t saved_change_count = 0;

    /// true while the active scene holds an edit that is only in memory. an undo back to the
    ///  saved state still counts: the count moved
    bool has_unsaved_scene_edits() const;
    /// the active scene is on disk as it is now
    void mark_scene_saved();

    /// what only a driver can do, set by the one that is the editor. unset, as where there
    ///  is no driver, asking for either does nothing
    std::function<void(const std::string_view)> send_playback_command = nullptr;
    std::function<bool()> save_scene = nullptr;

    /// one scene of the project as a list of them shows it
    struct scene_listing {
      natural_t id = 0;
      /// what the project calls it, and what the scene calls itself (its file)
      std::string name = "";
      std::string scene_name = "";
      bool active = false;
      bool starting = false;
    };

    /// set by the driver: the scenes that are loaded, in the scene graph's order
    std::function<void(ostd::vector<scene_listing>&)> list_scenes = nullptr;
    /// what list_scenes gave the last time it was asked
    ostd::vector<scene_listing> listed_scenes = {};
    /// a scene asked to be the active one. the driver makes it so after the frame, when
    ///  no window is in the middle of drawing the scene that goes
    opt<natural_t> pending_scene_activation = std::nullopt;

   private:
    /// records the scene as it is now against the baseline, when the two differ
    void record_since_baseline(const std::string_view name);

    uint64_t selection_changes = 0;
    ostd::vector<natural_t> selection_seen = {};
  };

}  // namespace other

#endif  // OTHER_EDITOR_SERVICES_EDITOR_SERVICES_HPP
