/**
 * \file ui/console/console_log_layout.cpp
 **/
#include "console_log_layout.hpp"

namespace other {
  namespace ui {
    namespace console_w {
      namespace {

        /// bytes >= 0x80 are part of a utf-8 multi-byte glyph: treated as word chars so words in
        ///  non-ascii text are kept together when wrapping
        bool is_word_char(char c) {
          const unsigned char uc = static_cast<unsigned char>(c);
          return uc >= 0x80 || std::isalnum(uc) != 0;
        }

        bool is_identifier_char(char c) {
          return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == ':';
        }

        bool is_newline_char(char c) {
          return c == '\n' || c == '\r';
        }

        /// length in bytes of the utf-8 sequence starting at text[pos] (1 for invalid/truncated input)
        uint32_t utf8_sequence_length(std::string_view text, size_t pos) {
          const unsigned char lead = static_cast<unsigned char>(text[pos]);
          uint32_t len = 1;
          if ((lead & 0xE0) == 0xC0) {
            len = 2;
          } else if ((lead & 0xF0) == 0xE0) {
            len = 3;
          } else if ((lead & 0xF8) == 0xF0) {
            len = 4;
          }

          if (pos + len > text.size()) {
            return 1;
          }
          for (uint32_t i = 1; i < len; ++i) {
            if ((static_cast<unsigned char>(text[pos + i]) & 0xC0) != 0x80) {
              return 1;
            }
          }
          return len;
        }

        bool is_type_span_candidate(std::string_view span) {
          if (span.size() < 3 || span.front() != '<' || span.back() != '>') {
            return false;
          }
          for (char c : span.substr(1, span.size() - 2)) {
            if (!is_identifier_char(c)) {
              return false;
            }
          }
          return true;
        }

        bool is_value_tuple_candidate(std::string_view span) {
          if (span.size() < 3 || span.front() != '(' || span.back() != ')') {
            return false;
          }
          for (char c : span.substr(1, span.size() - 2)) {
            if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
              return true;
            }
          }
          return false;
        }

        bool is_blank(std::string_view text) {
          for (char c : text) {
            if (c != ' ' && c != '\t') {
              return false;
            }
          }
          return true;
        }

        struct layout_builder {
          log_layout& out;
          float x = 0.f;

          void begin_line();

          void place(uint32_t offset, uint32_t length, float width, span_kind kind, uint32_t span_index);
        };

      }  // namespace

      ostd::vector<text_span> tokenize_log_message(const std::string_view msg) {
        ostd::vector<text_span> spans;

        size_t pos = 0;
        while (pos < msg.size()) {
          const char c = msg[pos];
          size_t len = 1;
          span_kind kind = span_kind::PLAIN;

          if (is_newline_char(c)) {
            kind = span_kind::NEWLINE;
            if (c == '\r' && pos + 1 < msg.size() && msg[pos + 1] == '\n') {
              len = 2;
            }
          } else if (is_word_char(c)) {
            while (pos + len < msg.size() && is_word_char(msg[pos + len])) {
              ++len;
            }
          } else if (c == '<' || c == '(') {
            const char close = c == '<' ? '>' : ')';
            size_t end = pos + 1;
            while (end < msg.size() && !is_newline_char(msg[end]) && msg[end] != '>' && msg[end] != ')') {
              ++end;
            }

            /// only a bracketed group that closes with the matching bracket and looks like a
            ///  type/value is taken whole; anything else falls back to a single plain char so
            ///  an unmatched bracket can not swallow (and block wrapping of) the rest of the line
            if (end < msg.size() && msg[end] == close) {
              const std::string_view candidate = msg.substr(pos, end + 1 - pos);
              if (c == '<' && is_type_span_candidate(candidate)) {
                kind = span_kind::TYPE;
                len = candidate.size();
              } else if (c == '(' && is_value_tuple_candidate(candidate)) {
                kind = span_kind::VALUE;
                len = candidate.size();
              }
            }
          } else {
            len = utf8_sequence_length(msg, pos);
          }

          spans.push_back({ .offset = static_cast<uint32_t>(pos), .length = static_cast<uint32_t>(len), .kind = kind });
          pos += len;
        }

        return spans;
      }

      void build_layout(std::string_view message, std::span<const text_span> spans, float wrap_width, float font_size, const measure_fn& measure, log_layout& layout) {
        layout.wrap_width = wrap_width;
        layout.font_size = font_size;
        layout.runs.clear();
        layout.lines.clear();

        const bool wrap = wrap_width > 0.f;

        layout_builder builder{ .out = layout };
        builder.begin_line();

        for (uint32_t i = 0; i < spans.size(); ++i) {
          const text_span& span = spans[i];
          if (span.kind == span_kind::NEWLINE) {
            builder.begin_line();
            continue;
          }

          const std::string_view text = message.substr(span.offset, span.length);
          const float width = measure(text);
          if (!wrap || builder.x + width <= wrap_width) {
            builder.place(span.offset, span.length, width, span.kind, i);
            continue;
          }

          /// does not fit on the current line
          if (is_blank(text)) {
            /// whitespace at a wrap point is consumed by the break
            builder.begin_line();
            continue;
          }

          if (width <= wrap_width) {
            builder.begin_line();
            builder.place(span.offset, span.length, width, span.kind, i);
            continue;
          }

          /// longer than a whole line (paths, hashes, ...): break between glyphs, filling the
          ///  current line first
          size_t pos = 0;
          while (pos < text.size()) {
            const uint32_t glyph_len = utf8_sequence_length(text, pos);
            const float glyph_width = measure(text.substr(pos, glyph_len));
            if (builder.x > 0.f && builder.x + glyph_width > wrap_width) {
              builder.begin_line();
            }
            builder.place(span.offset + static_cast<uint32_t>(pos), glyph_len, glyph_width, span.kind, i);
            pos += glyph_len;
          }
        }
      }

      namespace {

        void layout_builder::begin_line() {
          out.lines.push_back({ .first_run = static_cast<uint32_t>(out.runs.size()), .run_count = 0 });
          x = 0.f;
        }

        void layout_builder::place(uint32_t offset, uint32_t length, float width, span_kind kind, uint32_t span_index) {
          layout_line& line = out.lines.back();
          if (line.run_count > 0) {
            /// merge with the previous run when contiguous and same kind (fewer draw calls)
            layout_run& prev = out.runs.back();
            if (prev.span.kind == kind && prev.span.offset + prev.span.length == offset) {
              prev.span.length += length;
              prev.width += width;
              x += width;
              return;
            }
          }
          out.runs.push_back({
            .span = {
              .offset = offset,
              .length = length,
              .kind = kind,
            },
            .x = x,
            .width = width,
            .span_idx = span_index,
          });
          line.run_count++;
          x += width;
        }

      }  // namespace
    }  // namespace console_w
  }  // namespace ui
}  // namespace other