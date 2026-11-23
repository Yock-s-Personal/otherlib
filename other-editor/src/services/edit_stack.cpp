/**
 * \file services/edit_stack.cpp
 **/
#include "services/edit_stack.hpp"

#include "core/profiler.hpp"

namespace other {

  void edit_stack::commit(const edit& e) {
    OTHER_ASSERT(e.apply != nullptr, "Cannot commit edit: no apply action defined.");
    OTHER_ASSERT(e.undo != nullptr, "Cannot commit edit: no undo action defined.");
    PROFILE_SECTION("edit_stack::commit");
    e.apply();
    record(e);
  }

  void edit_stack::record(const edit& e) {
    OTHER_ASSERT(e.apply != nullptr, "Cannot record edit: no apply action defined.");
    OTHER_ASSERT(e.undo != nullptr, "Cannot record edit: no undo action defined.");
    PROFILE_SECTION("edit_stack::record");
    edit_future.clear();
    edit_history.push_back(e);
    history_bytes += e.bytes;
    ++changes;
    drop_oldest_past_the_limit();
  }

  void edit_stack::clear() {
    edit_history.clear();
    edit_future.clear();
    history_bytes = 0;
    changes = 0;
  }

  void edit_stack::undo() {
    PROFILE_SECTION("edit_stack::undo");
    if (can_undo()) {
      edit e = edit_history.back();
      OTHER_ASSERT(e.undo != nullptr, "Cannot undo edit: no undo action defined.");
      e.undo();
      edit_history.pop_back();
      history_bytes -= e.bytes;
      edit_future.push_back(e);
      ++changes;
    }
  }

  void edit_stack::redo() {
    PROFILE_SECTION("edit_stack::redo");
    if (can_redo()) {
      edit e = edit_future.back();
      OTHER_ASSERT(e.apply != nullptr, "Cannot redo edit: no apply action defined.");
      e.apply();
      edit_future.pop_back();
      edit_history.push_back(e);
      history_bytes += e.bytes;
      ++changes;
    }
  }

  std::string_view edit_stack::undo_name() const {
    return edit_history.empty() ? std::string_view{} : std::string_view{ edit_history.back().name };
  }

  void edit_stack::set_byte_limit(size_t limit) {
    byte_limit = limit;
    drop_oldest_past_the_limit();
  }

  void edit_stack::drop_oldest_past_the_limit() {
    size_t dropped = 0;
    while (history_bytes > byte_limit && edit_history.size() - dropped > 1) {
      history_bytes -= edit_history[dropped].bytes;
      ++dropped;
    }
    if (dropped > 0) {
      edit_history.erase(edit_history.begin(), edit_history.begin() + static_cast<std::ptrdiff_t>(dropped));
    }
  }

}  // namespace other
