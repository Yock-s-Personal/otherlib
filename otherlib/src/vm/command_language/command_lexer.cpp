/**
 * \file vm/command_language/command_lexer.cpp
 **/
#include "vm/command_language/command_lexer.hpp"

#include <charconv>

#include "core/profiler.hpp"

#include "vm/diagnostics/diagnostic_engine.hpp"

namespace other {
  namespace {

    struct keyword_row {
      std::string_view text;
      command_token_type type;
    };

    constexpr keyword_row kKeywords[] = {
      { "let", CMD_TOKEN_KW_LET },
      { "fn", CMD_TOKEN_KW_FN },
      { "return", CMD_TOKEN_KW_RETURN },
      { "if", CMD_TOKEN_KW_IF },
      { "else", CMD_TOKEN_KW_ELSE },
      { "while", CMD_TOKEN_KW_WHILE },
      { "break", CMD_TOKEN_KW_BREAK },
      { "continue", CMD_TOKEN_KW_CONTINUE },
      { "on", CMD_TOKEN_KW_ON },
      { "use", CMD_TOKEN_KW_USE },
      { "true", CMD_TOKEN_KW_TRUE },
      { "false", CMD_TOKEN_KW_FALSE },
      { "null", CMD_TOKEN_KW_NULL },
      { "error", CMD_TOKEN_KW_RESERVED },
      { "throw", CMD_TOKEN_KW_RESERVED },
      { "for", CMD_TOKEN_KW_RESERVED },
    };

    bool is_digit(const char c) {
      return c >= '0' && c <= '9';
    }

    bool is_name_start(const char c) {
      return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
    }

    bool is_name_char(const char c) {
      return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    }

  }  // namespace

  ostd::vector<command_token> command_lexer::tokenize(diagnostic_engine* diag) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine must not be null");
    PROFILE_SECTION("command_lexer::tokenize");
    diagnostics = diag;

    while (!finished()) {
      const char c = peek();
      if (c == ' ' || c == '\t' || c == '\r') {
        advance();
        space_pending = true;
        continue;
      }
      if (c == '#') {
        while (!finished() && peek() != '\n') {
          advance();
        }
        continue;
      }

      token_start = here();
      if (c == '\n') {
        lex_newline();
      } else if (is_digit(c)) {
        lex_number();
      } else if (is_name_start(c)) {
        lex_name();
      } else if (c == '"') {
        lex_string();
      } else {
        lex_operator();
      }
    }

    token_start = here();
    add(CMD_TOKEN_EOF, "");
    return std::move(tokens);
  }

  char command_lexer::advance() {
    const char c = source[cursor];
    ++cursor;
    ++column;
    return c;
  }

  void command_lexer::add(command_token_type type, std::string text, uint64_t value) {
    tokens.push_back({ .type = type, .text = std::move(text), .value = value, .span = { token_start, here() }, .space_before = space_pending });
    space_pending = false;
  }

  void command_lexer::error(vm_error_code code, std::string message) {
    had_error = true;
    diagnostics->emit({ .severity = VM_DIAGNOSTIC_ERROR, .error_code = code, .phase = VM_PHASE_LEXER, .span = { token_start, here() }, .final_message = std::move(message) });
  }

  void command_lexer::lex_newline() {
    advance();
    /// one newline token per run of blank lines, and none inside parentheses
    if (paren_depth == 0 && !tokens.empty() && tokens.back().type != CMD_TOKEN_NEWLINE) {
      add(CMD_TOKEN_NEWLINE, "\n");
    }
    ++line;
    column = 1;
    space_pending = true;
  }

  void command_lexer::lex_number() {
    const size_t start = cursor;
    int base = 10;
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
      advance();
      advance();
      base = 16;
    }

    const size_t digits = cursor;
    while (std::isxdigit(static_cast<unsigned char>(peek())) != 0 && (base == 16 || is_digit(peek()))) {
      advance();
    }

    if (peek() == '.' && is_digit(peek(1))) {
      while (is_digit(peek()) || peek() == '.') {
        advance();
      }
      error(LEX_INVALID_CHAR, std::format("'{}': numbers with a decimal point are not supported yet", source.substr(start, cursor - start)));
      return;
    }
    if (is_name_char(peek()) || cursor == digits) {
      while (is_name_char(peek())) {
        advance();
      }
      error(LEX_INVALID_CHAR, std::format("'{}' is not a number", source.substr(start, cursor - start)));
      return;
    }

    uint64_t value = 0;
    const std::from_chars_result parsed = std::from_chars(source.data() + digits, source.data() + cursor, value, base);
    if (parsed.ec != std::errc{}) {
      error(LEX_NUMBER_OUT_OF_RANGE, std::format("'{}' does not fit in 64 bits", source.substr(start, cursor - start)));
      return;
    }
    add(CMD_TOKEN_INTEGER, std::string{ source.substr(start, cursor - start) }, value);
  }

  void command_lexer::lex_name() {
    const size_t start = cursor;
    /// a hyphen belongs to the name only between two name characters, so 'a - b' and 'a -> b' still split
    while (is_name_char(peek()) || (peek() == '-' && is_name_char(peek(1)))) {
      advance();
    }

    const std::string_view text = source.substr(start, cursor - start);
    const auto keyword = std::ranges::find(kKeywords, text, &keyword_row::text);
    add(keyword != std::end(kKeywords) ? keyword->type : CMD_TOKEN_NAME, std::string{ text });
  }

  void command_lexer::lex_string() {
    advance();
    std::string text;
    while (!finished() && peek() != '"' && peek() != '\n') {
      const char c = advance();
      if (c != '\\') {
        text += c;
        continue;
      }

      const char escaped = finished() ? '\0' : advance();
      switch (escaped) {
        case 'n': text += '\n'; break;
        case 't': text += '\t'; break;
        case '0': text += '\0'; break;
        case '\\': text += '\\'; break;
        case '"': text += '"'; break;
        default:
          error(LEX_INVALID_CHAR, std::format("'\\{}' is not an escape; the escapes are \\n \\t \\0 \\\\ and \\\"", escaped));
          break;
      }
    }

    if (peek() != '"') {
      error(LEX_UNTERMINATED_STRING, "this string has no closing quote on its line");
      return;
    }
    advance();
    add(CMD_TOKEN_STRING, std::move(text));
  }

  void command_lexer::lex_operator() {
    const char c = advance();
    const auto either = [this](const char second, const command_token_type two, const std::string_view two_text, const command_token_type one, const std::string_view one_text) {
      if (peek() == second) {
        advance();
        add(two, std::string{ two_text });
      } else {
        add(one, std::string{ one_text });
      }
    };

    switch (c) {
      case '(':
        ++paren_depth;
        add(CMD_TOKEN_LEFT_PAREN, "(");
        break;
      case ')':
        paren_depth = paren_depth > 0 ? paren_depth - 1 : 0;
        add(CMD_TOKEN_RIGHT_PAREN, ")");
        break;
      case '{': add(CMD_TOKEN_LEFT_BRACE, "{"); break;
      case '}': add(CMD_TOKEN_RIGHT_BRACE, "}"); break;
      case ',': add(CMD_TOKEN_COMMA, ","); break;
      case ';': add(CMD_TOKEN_SEMICOLON, ";"); break;
      case '.': add(CMD_TOKEN_DOT, "."); break;
      case ':': add(CMD_TOKEN_COLON, ":"); break;
      case '+': add(CMD_TOKEN_PLUS, "+"); break;
      case '*': add(CMD_TOKEN_STAR, "*"); break;
      case '/': add(CMD_TOKEN_SLASH, "/"); break;
      case '%': add(CMD_TOKEN_PERCENT, "%"); break;
      case '-': either('>', CMD_TOKEN_ARROW, "->", CMD_TOKEN_MINUS, "-"); break;
      case '=': either('=', CMD_TOKEN_EQUAL_EQUAL, "==", CMD_TOKEN_ASSIGN, "="); break;
      case '!': either('=', CMD_TOKEN_BANG_EQUAL, "!=", CMD_TOKEN_BANG, "!"); break;
      case '<': either('=', CMD_TOKEN_LESS_EQUAL, "<=", CMD_TOKEN_LESS, "<"); break;
      case '>': either('=', CMD_TOKEN_GREATER_EQUAL, ">=", CMD_TOKEN_GREATER, ">"); break;
      case '|':
        if (peek() == '|') {
          advance();
          add(CMD_TOKEN_OR_OR, "||");
        } else {
          either('>', CMD_TOKEN_PIPE_GREATER, "|>", CMD_TOKEN_PIPE, "|");
        }
        break;
      case '&':
        if (peek() == '&') {
          advance();
          add(CMD_TOKEN_AND_AND, "&&");
        } else {
          error(LEX_INVALID_CHAR, "'&' by itself is not an operator; '&&' runs the next command when this one succeeds");
        }
        break;
      default:
        error(LEX_INVALID_CHAR, std::format("'{}' is not part of the language", c));
        break;
    }
  }

}  // namespace other
