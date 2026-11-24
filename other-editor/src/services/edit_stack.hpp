/**
 * \file services/edit_stack.hpp
 **/
#ifndef OTHER_EDITOR_SERVICES_EDIT_STACK_HPP
#define OTHER_EDITOR_SERVICES_EDIT_STACK_HPP

#include <functional>
#include <string>
#include <string_view>

#include "data-structures/std_container.hpp"

namespace other {

  struct edit {
    using action = std::function<void()>;
    action apply;
    action undo;
    /// what the user did, for a menu to say; empty for a value that was dragged
    std::string name = "";
    /// what keeping this edit costs, for the stack's limit
    size_t bytes = 0;
  };

  class edit_stack {
   public:
    /// records an edit whose mutation ALREADY happened (widget writes, snapshot pairs);
    ///  apply only runs on redo
    void record(const edit& e);

    void undo();
    void redo();

    void clear();

    inline bool can_undo() const { return !edit_history.empty(); }
    inline bool can_redo() const { return !edit_future.empty(); }
    /// the name of the edit an undo would undo; empty when there is none or it has none
    std::string_view undo_name() const;
    size_t undo_count() const { return edit_history.size(); }

    /// goes up with every edit, undo and redo, and back to zero on clear. two equal counts
    ///  mean nothing was changed in between
    inline uint64_t change_count() const { return changes; }

    /// the most the edits that can be undone may cost together. past it the oldest go,
    ///  though never the newest: one edit can always be undone
    void set_byte_limit(size_t limit);
    size_t kept_bytes() const { return history_bytes; }

    constexpr static size_t kDefaultByteLimit = 256ull * 1024ull * 1024ull;

   private:
    void drop_oldest_past_the_limit();

    ostd::vector<edit> edit_history;
    ostd::vector<edit> edit_future;
    uint64_t changes = 0;
    size_t history_bytes = 0;
    size_t byte_limit = kDefaultByteLimit;
  };

}  // namespace other

#endif  // OTHER_EDITOR_SERVICES_EDIT_STACK_HPP
