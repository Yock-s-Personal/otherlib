/**
 * \file services/editor_services.cpp
 **/
#include "services/editor_services.hpp"

#include <algorithm>
#include <format>

#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "object/camera_component.hpp"
#include "object/light_component.hpp"
#include "object/render_component.hpp"

#include "services/scene_pick.hpp"

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

  void editor_services::drop_edit() {
    PROFILE_SECTION("editor_services::drop_edit");
    if (edit_tracker.open_edits == 0) {
      CORE_LOG_ERROR("An edit was dropped that was not begun.");
      return;
    }
    if (--edit_tracker.open_edits > 0) {
      return;
    }

    edit_tracker.open_edit_name.clear();
    scene* s = current_selection.scene_ptr;
    if (s == nullptr || s->is_playing() || edit_tracker.baseline.empty()) {
      return;
    }
    /// the scene as it is now is what the next action is measured from. putting a value
    ///  back need not put back every bit of what holds it, and that is no change to undo
    edit_tracker.baseline = s->capture_snapshot();
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

  bool editor_services::focus_view_on_selection() {
    PROFILE_SECTION("editor_services::focus_view_on_selection");
    if (view_camera == nullptr || !has_selection() || current_selection.scene_ptr == nullptr) {
      return false;
    }
    camera_controller.focus(*view_camera, get_selection_bounding_box());
    return true;
  }

  void editor_services::collect_view_icons(ostd::vector<view_icon>& icons) {
    PROFILE_SECTION("editor_services::collect_view_icons");
    icons.clear();
    scene* s = current_selection.scene_ptr;
    if (view_camera == nullptr || s == nullptr || view.size.x < 1.f || view.size.y < 1.f) {
      return;
    }

    const auto add = [&](natural_t object, view_icon::kind what, const glm::vec3& at) -> void {
      if (!s->is_visible(object)) {
        return;
      }
      const opt<glm::vec2> place = view_place(*view_camera, view.size, at);
      if (!place.has_value()) {
        return;
      }
      icons.push_back({
        .object = object,
        .what = what,
        .place = *place,
        .selected = std::ranges::find(current_selection.objects, object) != current_selection.objects.end(),
      });
    };
    /// a point light shines from a place of its own within its object; a camera is where
    ///  it says it is, which is not its object's place
    s->each_component<point_light_component>([&](const auto& handle, const point_light_component& light) -> void {
      add(handle.id, view_icon::POINT_LIGHT, glm::vec3(s->get_world_transform(handle.id) * glm::vec4(light.light.position, 1.f)));
    });
    s->each_component<direction_light_component>([&](const auto& handle, const direction_light_component&) -> void {
      add(handle.id, view_icon::DIRECTION_LIGHT, glm::vec3(s->get_world_transform(handle.id)[3]));
    });
    s->each_component<camera_component>([&](const auto& handle, const camera_component& cam) -> void {
      add(handle.id, view_icon::CAMERA, cam.camera.position);
    });

    /// an object that is more than one of these has them at one place more often than not:
    ///  a camera with a light on it is both where the camera is. drawn there, the last
    ///  would cover the rest, so those of one object that would overlap are set side by
    ///  side instead, in a row with its middle where they all were
    constexpr float kApart = 2.f * kIconReach + 2.f;
    ostd::vector<uint8_t> set_out(icons.size(), 0);
    ostd::vector<size_t> row;
    for (size_t i = 0; i < icons.size(); ++i) {
      if (set_out[i] != 0) {
        continue;
      }
      row.clear();
      row.push_back(i);
      glm::vec2 middle = icons[i].place;
      for (size_t j = i + 1; j < icons.size(); ++j) {
        if (set_out[j] == 0 && icons[j].object == icons[i].object && glm::distance(icons[j].place, icons[i].place) < kApart) {
          row.push_back(j);
          middle += icons[j].place;
        }
      }
      if (row.size() < 2) {
        continue;
      }

      middle /= static_cast<float>(row.size());
      for (size_t k = 0; k < row.size(); ++k) {
        const float along = (static_cast<float>(k) - static_cast<float>(row.size() - 1) * 0.5f) * kApart;
        icons[row[k]].place = { middle.x + along, middle.y };
        set_out[row[k]] = 1;
      }
    }
  }

  natural_t editor_services::place_model(const std::string_view path, const glm::vec2& screen_place) {
    PROFILE_SECTION("editor_services::place_model");
    scene* s = current_selection.scene_ptr;
    if (view_camera == nullptr || s == nullptr || view.size.x < 1.f || view.size.y < 1.f || !begin_asset_load || path.empty()) {
      return 0;
    }
    const natural_t model = begin_asset_load(path);
    if (model == 0) {
      CORE_LOG_ERROR("The model '{}' was not placed in the scene: it could not be loaded.", path);
      return 0;
    }

    /// how far in front of the camera it is put when the pointer is over nothing at all
    constexpr float kInFront = 5.f;
    const pick_ray r = view_ray(*view_camera, view.size, screen_place - view.position);
    glm::vec3 at = r.origin + r.direction * kInFront;
    if (const opt<pick_hit> hit = pick_object(*s, r); hit.has_value()) {
      at = r.origin + r.direction * hit->distance;
    } else if (r.direction.y < -1e-4f && r.origin.y > 0.f) {
      /// the ground is where the grid is drawn: y is 0. only when it is in sight
      const float to_ground = -r.origin.y / r.direction.y;
      if (to_ground < view_camera->clip.far_plane) {
        at = r.origin + r.direction * to_ground;
      }
    }

    const std::string name = filepath{ path }.stem().string();
    begin_edit(std::format("Add {}", name));
    scene_object& placed = s->create_object(name, at);
    render_component render = {};
    render.model_asset_id = model;
    s->add_component<render_component>(&placed, std::move(render));
    const natural_t id = placed.id;
    end_edit();
    set_selection(id);
    return id;
  }

  natural_t editor_services::pick_in_view(const glm::vec2& screen_place) {
    PROFILE_SECTION("editor_services::pick_in_view");
    scene* s = current_selection.scene_ptr;
    if (view_camera == nullptr || s == nullptr || view.size.x < 1.f || view.size.y < 1.f) {
      return 0;
    }

    /// an icon is drawn over everything, so one under the pointer is what is meant: the
    ///  one whose middle is nearest, of those in reach
    ostd::vector<view_icon> icons;
    collect_view_icons(icons);
    natural_t on_icon = 0;
    float nearest = kIconReach;
    for (const view_icon& icon : icons) {
      const float distance = glm::distance(icon.place, screen_place - view.position);
      if (distance <= nearest) {
        nearest = distance;
        on_icon = icon.object;
      }
    }
    if (on_icon != 0) {
      return on_icon;
    }
    const opt<pick_hit> hit = pick_object(*s, view_ray(*view_camera, view.size, screen_place - view.position));
    return hit.has_value() ? hit->object : 0;
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
