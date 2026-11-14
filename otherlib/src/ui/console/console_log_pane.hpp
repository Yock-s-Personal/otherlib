/**
 * \file ui/console/console_log_pane.hpp
 **/
#ifndef OTHERLIB_UI_CONSOLE_CONSOLE_LOG_PANE_HPP
#define OTHERLIB_UI_CONSOLE_CONSOLE_LOG_PANE_HPP

#include "ui/console/console_widgets.hpp"

namespace other {
  namespace ui {
    namespace console_w {

      /// rows taller than this are cut off in the list; the detail pane always shows everything
      constexpr uint32_t kMaxCollapsedLines = 6;
      /// below this the message column stops shrinking (avoids one-glyph-per-line rows)
      constexpr float kMinWrapWidth = 120.f;
      constexpr float kDetailPaneHeight = 160.f;
      constexpr float kDetailPaneMaxFraction = 0.5f;

      struct log_action {
        std::string label;
        std::function<bool(const log_entry&)> is_available = nullptr;
        std::function<void(const log_entry&)> invoke = nullptr;
      };

      struct log_pane_result {
        bool hovered = false;
        bool clicked = false;
        bool context_requested = false;
      };

      struct detail_pane_result {
        bool close_requested = false;
      };

      log_pane_result draw_log_pane(log_entry& entry, bool alt_row, bool selected);
      detail_pane_result draw_detail_pane(const log_entry& entry, std::span<const log_action> actions);

      bool draw_log_action_menu(const log_entry& entry, std::span<const log_action> actions);

    }  // namespace console_w
  }  // namespace ui
}  // namespace other

#endif  // OTHERLIB_UI_CONSOLE_CONSOLE_LOG_PANE_HPP