/**
 * \file tests/ui/console_log_layout_tests.cpp
 **/
#include "console_log_layout_tests.hpp"

#include "ui/console/console_log_layout.hpp"

namespace other {

  using namespace ui::console_w;

  namespace {

    struct laid_out {
      std::string message;
      ostd::vector<text_span> spans;
      log_layout layout;

      std::string line_text(size_t line_idx) const;
    };

    float measure_mono(std::string_view text);
    laid_out lay_out(std::string message, float wrap_width);

  }  // namespace

  TEST_F(console_log_layout_tests, short_message_is_one_line) {
    laid_out r = lay_out("hello world", 40.f);
    ASSERT_EQ(r.layout.lines.size(), 1u);
    ASSERT_EQ(r.line_text(0), "hello world");
    /// contiguous plain text merges into a single run
    ASSERT_EQ(r.layout.runs.size(), 1u);
  }

  TEST_F(console_log_layout_tests, empty_message_has_one_empty_line) {
    laid_out r = lay_out("", 40.f);
    ASSERT_EQ(r.layout.lines.size(), 1u);
    ASSERT_EQ(r.layout.lines[0].run_count, 0u);
  }

  TEST_F(console_log_layout_tests, wraps_at_word_boundaries) {
    laid_out r = lay_out("unmatched ( paren with a lot of text after it", 20.f);
    ASSERT_EQ(r.layout.lines.size(), 3u);
    ASSERT_EQ(r.line_text(0), "unmatched ( paren ");
    ASSERT_EQ(r.line_text(1), "with a lot of text ");
    ASSERT_EQ(r.line_text(2), "after it");
  }

  TEST_F(console_log_layout_tests, lines_never_exceed_wrap_width) {
    laid_out r = lay_out("Loaded <scene::node> at (0, 1.5, 0) from C:/Users/someone/projects/game/assets/levels/level_01.oscene ok", 30.f);
    ASSERT_GT(r.layout.lines.size(), 1u);
    for (const layout_line& line : r.layout.lines) {
      float x = 0.f;
      for (uint32_t i = line.first_run; i < line.first_run + line.run_count; ++i) {
        ASSERT_FLOAT_EQ(r.layout.runs[i].x, x);
        x += r.layout.runs[i].width;
      }
      ASSERT_LE(x, 30.f);
    }
  }

  TEST_F(console_log_layout_tests, long_token_breaks_between_glyphs) {
    laid_out r = lay_out(std::string(25, 'A'), 10.f);
    ASSERT_EQ(r.layout.lines.size(), 3u);
    ASSERT_EQ(r.line_text(0), std::string(10, 'A'));
    ASSERT_EQ(r.line_text(2), std::string(5, 'A'));
  }

  TEST_F(console_log_layout_tests, explicit_newlines_are_kept) {
    laid_out r = lay_out("one\r\ntwo\n\nfour", 40.f);
    ASSERT_EQ(r.layout.lines.size(), 4u);
    ASSERT_EQ(r.line_text(0), "one");
    ASSERT_EQ(r.line_text(1), "two");
    ASSERT_EQ(r.line_text(2), "");
    ASSERT_EQ(r.line_text(3), "four");
  }

  TEST_F(console_log_layout_tests, zero_width_disables_wrapping) {
    laid_out r = lay_out("this would wrap at any sensible width but must stay on one line", 0.f);
    ASSERT_EQ(r.layout.lines.size(), 1u);
  }

  TEST_F(console_log_layout_tests, glyph_wider_than_wrap_width_still_terminates) {
    laid_out r = lay_out("WWWW", 0.5f);
    ASSERT_EQ(r.layout.lines.size(), 4u);
  }

  TEST_F(console_log_layout_tests, utf8_glyphs_are_never_split) {
    /// "café — déjà vu": é, à are 2 bytes, the em dash is 3
    laid_out r = lay_out("caf\xc3\xa9 \xe2\x80\x94 d\xc3\xa9j\xc3\xa0 vu", 4.f);
    for (const layout_run& run : r.layout.runs) {
      const auto first = static_cast<unsigned char>(r.message[run.span.offset]);
      ASSERT_NE(first & 0xC0, 0x80) << "run starts inside a utf-8 sequence";
      const size_t end = run.span.offset + run.span.length;
      if (end < r.message.size()) {
        ASSERT_NE(static_cast<unsigned char>(r.message[end]) & 0xC0, 0x80) << "run ends inside a utf-8 sequence";
      }
    }
  }

  TEST_F(console_log_layout_tests, type_and_value_spans_are_classified) {
    const std::string message = "got <scene::node> at (0, 1.5, 0) <not a type> (no digits)";
    ostd::vector<text_span> spans = tokenize_log_message(message);

    size_t types = 0;
    size_t values = 0;
    for (const text_span& span : spans) {
      const std::string text = message.substr(span.offset, span.length);
      if (span.kind == span_kind::TYPE) {
        ASSERT_EQ(text, "<scene::node>");
        ++types;
      } else if (span.kind == span_kind::VALUE) {
        ASSERT_EQ(text, "(0, 1.5, 0)");
        ++values;
      }
    }
    ASSERT_EQ(types, 1u);
    ASSERT_EQ(values, 1u);
  }

  TEST_F(console_log_layout_tests, layout_is_only_valid_for_its_width_and_font) {
    laid_out r = lay_out("hello", 40.f);
    ASSERT_TRUE(r.layout.valid_for(40.f, 13.f));
    ASSERT_FALSE(r.layout.valid_for(41.f, 13.f));
    ASSERT_FALSE(r.layout.valid_for(40.f, 14.f));
    ASSERT_FALSE(log_layout{}.valid_for(-1.f, 0.f));
  }

  namespace {

    std::string laid_out::line_text(size_t line_idx) const {
      std::string out;
      const layout_line& line = layout.lines[line_idx];
      for (uint32_t i = line.first_run; i < line.first_run + line.run_count; ++i) {
        out += message.substr(layout.runs[i].span.offset, layout.runs[i].span.length);
      }
      return out;
    }

    float measure_mono(std::string_view text) {
      float width = 0.f;
      for (char c : text) {
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) {
          width += 1.f;
        }
      }
      return width;
    }

    laid_out lay_out(std::string message, float wrap_width) {
      laid_out result;
      result.message = std::move(message);
      result.spans = tokenize_log_message(result.message);
      build_layout(result.message, result.spans, wrap_width, 13.f, measure_mono, result.layout);
      return result;
    }

  }  // namespace
}  // namespace other