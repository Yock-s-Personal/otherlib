/**
 * \file ui/script_window.hpp
 **/
#ifndef OTHERLIB_UI_SCRIPT_WINDOW_HPP
#define OTHERLIB_UI_SCRIPT_WINDOW_HPP

#include <string>
#include <string_view>

#include <glm/glm.hpp>

#include "core/defines.hpp"

#include "dotnet/types.hpp"

#include "ui/ui_window.hpp"

namespace other {
  namespace ui {

    /// a window whose body is drawn by a script. everything else about it is ui_window's:
    ///  the frame, the close button, docking, the layout, when it is shown
    class script_window : public ui_window {
     public:
      /// draws the body of the script's window with that id; false when the script threw
      using draw_fn = nbool32 (*)(natural_t window_id);

      /// name is what the window is opened by, and what its errors call it
      script_window(event_system* events, const std::string_view name, const std::string_view title, natural_t window_id, glm::vec2 default_size, int32_t flags);

      /// one function draws every script window. null while no script has handed one over
      static void set_draw_function(draw_fn fn);

      /// how many times the script was asked to draw this window, and how long the last took
      natural_t draw_count() const { return draws; }
      float last_draw_microseconds() const { return last_draw_us; }

      /// a window whose script threw, or drew in a way ImGui objected to, is not asked to
      ///  draw again: the error is said once, and the window says why it is blank
      bool is_disabled() const { return disabled; }
      const std::string& disabled_reason() const { return reason; }
      /// the script is asked again from the next frame, and stopped again if it still fails
      void enable();

     protected:
      void on_prepare_render() override;
      void on_render_body() override;

     private:
      std::string name;
      natural_t window_id = 0;
      glm::vec2 default_size = { 0.f, 0.f };

      natural_t draws = 0;
      float last_draw_us = 0.f;

      bool disabled = false;
      std::string reason;

      void render_disabled_notice();
    };

  }  // namespace ui
}  // namespace other

#endif  // OTHERLIB_UI_SCRIPT_WINDOW_HPP
