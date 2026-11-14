/**
 * \file ui/console/console_log_layout.hpp
 **/
#ifndef OTHERLIB_UI_CONSOLE_CONSOLE_LOG_LAYOUT_HPP
#define OTHERLIB_UI_CONSOLE_CONSOLE_LOG_LAYOUT_HPP

namespace other {
  namespace ui {
    namespace console_w {

      enum class span_kind : uint8_t {
        PLAIN = 0,
        TYPE,
        VALUE,
        NEWLINE,
      };

      struct text_span {
        uint32_t offset = 0;
        uint32_t length = 0;
        span_kind kind = span_kind::PLAIN;
      };

      struct layout_run {
        text_span span;

        float x = 0.f;
        float width = 0.f;

        uint32_t span_idx = 0;
      };

      struct layout_line {
        uint32_t first_run = 0;
        uint32_t run_count = 0;
      };

      struct log_layout {
        float wrap_width = 0.f;
        float font_size = 0.f;

        ostd::vector<layout_run> runs;
        ostd::vector<layout_line> lines;

        bool valid_for(float width, float size) const {
          return !lines.empty() && wrap_width == width && font_size == size;
        }
      };

      using measure_fn = std::function<float(std::string_view)>;

      ostd::vector<text_span> tokenize_log_message(const std::string_view msg);

      void build_layout(std::string_view message, std::span<const text_span> spans, float wrap_width, float font_size, const measure_fn& measure, log_layout& layout);

    }  // namespace console_w
  }  // namespace ui
}  // namespace other

#endif  // OTHERLIB_UI_CONSOLE_CONSOLE_LOG_LAYOUT_HPP