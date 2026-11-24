/**
 * \file ui/script_draw_guard.hpp
 **/
#ifndef OTHERLIB_UI_SCRIPT_DRAW_GUARD_HPP
#define OTHERLIB_UI_SCRIPT_DRAW_GUARD_HPP

#include <cstdint>
#include <functional>
#include <string>

namespace other {
  namespace ui {

    /// what ImGui objected to while a script drew: the first objection, and how many
    struct script_draw_errors {
      std::string first;
      uint32_t count = 0;

      /// the objections as one line for a log or a notice. empty when there were none
      std::string describe() const;
    };

    /// calls a script that draws with ImGui, inside the window being drawn. a script's
    ///  mistake with ImGui must not stop the driver, or be said every frame: while the
    ///  script draws, ImGui hands what it objects to here (no assert, and nothing in its
    ///  own log or tooltip), and what the script left open (a clip rectangle, a tree node,
    ///  a pushed id) is closed afterwards, each as an objection. the script is expected to
    ///  catch what it throws itself: nothing may unwind through here
    script_draw_errors guard_script_draw(const std::function<void()>& draw);

  }  // namespace ui
}  // namespace other

#endif  // OTHERLIB_UI_SCRIPT_DRAW_GUARD_HPP
