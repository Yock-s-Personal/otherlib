/**
 * \file services/editor_services.cpp
 **/
#include "services/editor_services.hpp"

#include <algorithm>

#include "core/logger.hpp"
#include "core/profiler.hpp"

namespace other {

  namespace {

    /// a snapshot-pair edit: undo restores `before`, redo restores `after`; scene resolved at
    ///  invoke time (never a captured pointer), selection carried by name since restore reassigns runtime ids
    edit make_snapshot_edit(editor_services* services, const std::string_view name, ostd::vector<uint8_t> before, ostd::vector<uint8_t> after) {
      auto restore = [services](const ostd::vector<uint8_t>& snapshot) {
        scene* s = services->current_selection.scene_ptr;
        if (s == nullptr) {
          CORE_LOG_ERROR("Cannot restore scene edit: no active scene.");
          return;
        }

        ostd::vector<std::string> selected_names = {};
        for (const natural_t obj_id : services->current_selection.objects) {
          const scene_object* obj = s->find_object(obj_id);
          if (obj != nullptr) {
            selected_names.push_back(obj->name);
          }
        }

        s->restore_snapshot(snapshot);

        /// objects absent from the restored state (e.g. undoing a create) drop out
        services->current_selection.objects.clear();
        for (const std::string& obj_name : selected_names) {
          const scene_object* obj = s->find_object(obj_name);
          if (obj != nullptr && std::ranges::find(services->current_selection.objects, obj->id) == services->current_selection.objects.end()) {
            services->current_selection.objects.push_back(obj->id);
          }
        }

        services->edit_tracker.baseline = s->capture_snapshot();
      };

      const size_t bytes = before.size() + after.size();
      return edit{
        .apply = [restore, after = std::move(after)]() { restore(after); },
        .undo = [restore, before = std::move(before)]() { restore(before); },
        .name = std::string{ name },
        .bytes = bytes,
      };
    }

  }  // namespace

  void editor_services::notify_scene_edited() {
    PROFILE_SECTION("editor_services::notify_scene_edited");
    scene* s = current_selection.scene_ptr;
    if (s == nullptr || s->is_playing()) {
      return;
    }
    /// inside begin_edit and end_edit the pair is what records
    if (edit_tracker.open_edits > 0) {
      return;
    }

    if (!edit_tracker.session_active) {
      if (edit_tracker.baseline.empty()) {
        /// no baseline means we cannot reconstruct the pre-edit state; skip this session
        CORE_LOG_WARN("Scene edit began without a baseline snapshot; this edit will not be undoable.");
        edit_tracker.baseline = s->capture_snapshot();
        return;
      }
      edit_tracker.session_active = true;
    }
    edit_tracker.idle_frames = 0;
  }

  void editor_services::record_since_baseline(const std::string_view name) {
    scene* s = current_selection.scene_ptr;
    OTHER_ASSERT(s != nullptr, "An edit is recorded against the active scene, and there is none.");
    ostd::vector<uint8_t> after = s->capture_snapshot();
    if (after != edit_tracker.baseline) {
      editing_history.record(make_snapshot_edit(this, name, edit_tracker.baseline, after));
      edit_tracker.baseline = std::move(after);
    }
  }

  void editor_services::tick_edit_tracker() {
    PROFILE_SECTION("editor_services::tick_edit_tracker");
    scene* s = current_selection.scene_ptr;
    if (s == nullptr) {
      return;
    }

    /// keep a baseline ready from the first quiet frame so the next session has a
    ///  true pre-edit state to restore to
    if (!edit_tracker.session_active) {
      if (edit_tracker.baseline.empty() && !s->is_playing()) {
        edit_tracker.baseline = s->capture_snapshot();
      }
      return;
    }

    if (++edit_tracker.idle_frames < scene_edit_tracker::kIdleFramesToCommit) {
      return;
    }

    record_since_baseline("");
    edit_tracker.session_active = false;
    edit_tracker.idle_frames = 0;
  }

  void editor_services::reset_scene_edit_tracking() {
    PROFILE_SECTION("editor_services::reset_scene_edit_tracking");
    editing_history.clear();
    edit_tracker.baseline.clear();
    edit_tracker.session_active = false;
    edit_tracker.idle_frames = 0;
    edit_tracker.open_edits = 0;
    edit_tracker.open_edit_name.clear();
    saved_change_count = 0;
  }

  void editor_services::begin_edit(const std::string_view name) {
    PROFILE_SECTION("editor_services::begin_edit");
    if (edit_tracker.open_edits++ > 0) {
      return;
    }
    edit_tracker.open_edit_name = name;

    scene* s = current_selection.scene_ptr;
    if (s == nullptr || s->is_playing()) {
      return;
    }
    /// a value still being dragged is its own step, closed before this one opens
    if (edit_tracker.session_active) {
      record_since_baseline("");
      edit_tracker.session_active = false;
      edit_tracker.idle_frames = 0;
    }
    if (edit_tracker.baseline.empty()) {
      edit_tracker.baseline = s->capture_snapshot();
    }
  }

  void editor_services::end_edit() {
    PROFILE_SECTION("editor_services::end_edit");
    if (edit_tracker.open_edits == 0) {
      CORE_LOG_ERROR("An edit was ended that was not begun.");
      return;
    }
    if (--edit_tracker.open_edits > 0) {
      return;
    }

    const std::string name = std::move(edit_tracker.open_edit_name);
    edit_tracker.open_edit_name.clear();
    scene* s = current_selection.scene_ptr;
    if (s == nullptr || s->is_playing() || edit_tracker.baseline.empty()) {
      return;
    }
    record_since_baseline(name);
  }

  bool editor_services::has_unsaved_scene_edits() const {
    return edit_tracker.session_active || editing_history.change_count() != saved_change_count;
  }

  void editor_services::mark_scene_saved() {
    saved_change_count = editing_history.change_count();
  }

  void editor_services::undo_scene_edit() {
    PROFILE_SECTION("editor_services::undo_scene_edit");
    scene* s = current_selection.scene_ptr;
    if (s == nullptr || s->is_playing()) {
      return;
    }
    if (edit_tracker.session_active) {
      edit_tracker.idle_frames = scene_edit_tracker::kIdleFramesToCommit;
      tick_edit_tracker();
    }
    if (!editing_history.can_undo()) {
      CORE_LOG_DEBUG("Nothing to undo.");
      return;
    }
    editing_history.undo();
  }

  void editor_services::redo_scene_edit() {
    PROFILE_SECTION("editor_services::redo_scene_edit");
    scene* s = current_selection.scene_ptr;
    if (s == nullptr || s->is_playing()) {
      return;
    }
    if (!editing_history.can_redo()) {
      CORE_LOG_DEBUG("Nothing to redo.");
      return;
    }
    editing_history.redo();
  }

  bool editor_services::can_undo_scene_edit() const {
    const scene* s = current_selection.scene_ptr;
    return s != nullptr && !s->is_playing() && (edit_tracker.session_active || editing_history.can_undo());
  }

  bool editor_services::can_redo_scene_edit() const {
    const scene* s = current_selection.scene_ptr;
    return s != nullptr && !s->is_playing() && !edit_tracker.session_active && editing_history.can_redo();
  }

  void editor_services::capture_playback_selection() {
    PROFILE_SECTION("editor_services::capture_playback_selection");
    playback_selection_names.clear();
    scene* s = current_selection.scene_ptr;
    if (s == nullptr) {
      return;
    }

    for (const natural_t obj_id : current_selection.objects) {
      const scene_object* obj = s->find_object(obj_id);
      if (obj != nullptr) {
        playback_selection_names.push_back(obj->name);
      }
    }
  }

  void editor_services::restore_playback_selection() {
    PROFILE_SECTION("editor_services::restore_playback_selection");
    scene* s = current_selection.scene_ptr;
    if (s == nullptr) {
      return;
    }

    /// every pre-stop id is dead after the restore; objects absent from the restored
    ///  state (e.g. selected while playing after being spawned by a script) drop out
    current_selection.objects.clear();
    for (const std::string& obj_name : playback_selection_names) {
      const scene_object* obj = s->find_object(obj_name);
      if (obj != nullptr && std::ranges::find(current_selection.objects, obj->id) == current_selection.objects.end()) {
        current_selection.objects.push_back(obj->id);
      }
    }
    playback_selection_names.clear();
  }

  void editor_services::select_object(natural_t object_id) {
    if (current_selection.scene_ptr == nullptr) {
      CORE_LOG_ERROR("Cannot select object with ID {} because there is no active scene.", object_id);
      return;
    }

    if (!current_selection.multiple_selection_enabled) {
      current_selection.objects.clear();
    }

    current_selection.objects.push_back(object_id);
  }

  void editor_services::deselect_object(natural_t object_id) {
    if (current_selection.scene_ptr == nullptr) {
      CORE_LOG_ERROR("Cannot deselect object with ID {} because there is no active scene.", object_id);
      return;
    }

    auto& objects = current_selection.objects;
    auto it = std::ranges::find(objects, object_id);
    if (it != objects.end()) {
      objects.erase(it);
    } else {
      CORE_LOG_WARN("Tried to deselect object with ID {}, but it was not in the current selection.", object_id);
    }
  }

  bool editor_services::has_selection() const {
    return current_selection.scene_ptr != nullptr &&
      !current_selection.objects.empty();
  }

  bool editor_services::multi_select_enabled() const {
    return current_selection.multiple_selection_enabled;
  }

  bounding_box editor_services::get_selection_bounding_box() const {
    PROFILE_SECTION("editor_services::get_selection_bounding_box");
    if (!has_selection()) {
      return {};
    }

    bounding_box result = bounding_box::empty;
    for (natural_t obj_id : current_selection.objects) {
      result = bounding_box::expand_to_include(result, current_selection.scene_ptr->get_bounding_box(obj_id));
    }

    if (result == bounding_box::empty) {
      result = bounding_box(glm::vec3(-1.f), glm::vec3(1.f));
    }
    return result;
  }

  void editor_services::set_selection(natural_t object_id) {
    current_selection.objects.clear();
    add_to_selection(object_id);
    tick_selection();
  }

  void editor_services::add_to_selection(natural_t object_id) {
    const scene* s = current_selection.scene_ptr;
    if (s == nullptr || object_id == 0 || !s->has_object(object_id)) {
      tick_selection();
      return;
    }
    /// selected again, it becomes the active one
    std::erase(current_selection.objects, object_id);
    current_selection.objects.push_back(object_id);
    tick_selection();
  }

  void editor_services::remove_from_selection(natural_t object_id) {
    std::erase(current_selection.objects, object_id);
    tick_selection();
  }

  void editor_services::clear_selection() {
    current_selection.objects.clear();
    tick_selection();
  }

  natural_t editor_services::active_selection() const {
    return has_selection() ? current_selection.objects.back() : 0;
  }

  void editor_services::tick_selection() {
    const scene* s = current_selection.scene_ptr;
    if (s == nullptr) {
      current_selection.objects.clear();
    } else {
      std::erase_if(current_selection.objects, [s](natural_t object_id) -> bool {
        return !s->has_object(object_id);
      });
    }
    if (current_selection.objects != selection_seen) {
      selection_seen = current_selection.objects;
      ++selection_changes;
    }
  }

  bool editor_services::pick_folder() {
    if (!show_folder_dialog) {
      return false;
    }
    show_folder_dialog();
    return true;
  }

  opt<std::string> editor_services::take_picked_folder() {
    const std::lock_guard lock(picked_mutex);
    opt<std::string> taken = std::move(picked_folder);
    picked_folder.reset();
    return taken;
  }

  void editor_services::folder_was_picked(const std::string_view folder) {
    const std::lock_guard lock(picked_mutex);
    picked_folder = std::string{ folder };
  }

}  // namespace other
