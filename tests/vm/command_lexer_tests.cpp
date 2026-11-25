/**
 * \file tests/vm/command_lexer_tests.cpp
 **/
#include "vm/command_language/command_lexer.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"

#include "other_test.hpp"

namespace other {

  class command_lexer_tests : public other_test {
   protected:
    struct lexed {
      ostd::vector<command_token> tokens;
      bool failed = false;
    };

    static lexed lex(const std::string_view text) {
      diagnostic_engine diag;
      command_lexer lexer{ text };
      lexed result = { .tokens = lexer.tokenize(&diag) };
      result.failed = lexer.failed();
      return result;
    }

    static ostd::vector<command_token_type> types(const lexed& result) {
      ostd::vector<command_token_type> found;
      for (const command_token& token : result.tokens) {
        found.push_back(token.type);
      }
      return found;
    }
  };

  TEST_F(command_lexer_tests, a_command_line_becomes_names_and_literals) {
    const lexed result = lex("core log \"hello\" 2\n");
    ASSERT_FALSE(result.failed);
    const ostd::vector<command_token_type> expected = { CMD_TOKEN_NAME, CMD_TOKEN_NAME, CMD_TOKEN_STRING, CMD_TOKEN_INTEGER, CMD_TOKEN_NEWLINE, CMD_TOKEN_EOF };
    EXPECT_EQ(types(result), expected);
    EXPECT_EQ(result.tokens[2].text, "hello");
    EXPECT_EQ(result.tokens[3].value, 2);
  }

  TEST_F(command_lexer_tests, a_word_of_hex_digits_is_a_name) {
    /// the OASM lexer reads these as numbers, which is why it is not shared
    const lexed result = lex("let face = add");
    ASSERT_FALSE(result.failed);
    EXPECT_EQ(result.tokens[1].type, CMD_TOKEN_NAME);
    EXPECT_EQ(result.tokens[1].text, "face");
    EXPECT_EQ(result.tokens[3].type, CMD_TOKEN_NAME);
  }

  TEST_F(command_lexer_tests, a_hyphen_between_name_characters_joins_them) {
    const lexed joined = lex("add-three");
    ASSERT_EQ(joined.tokens.size(), 2);
    EXPECT_EQ(joined.tokens[0].text, "add-three");

    const lexed spaced = lex("a - b");
    const ostd::vector<command_token_type> minus = { CMD_TOKEN_NAME, CMD_TOKEN_MINUS, CMD_TOKEN_NAME, CMD_TOKEN_EOF };
    EXPECT_EQ(types(spaced), minus);

    const lexed arrow = lex("f() -> int");
    EXPECT_EQ(arrow.tokens[3].type, CMD_TOKEN_ARROW);
  }

  TEST_F(command_lexer_tests, numbers_are_decimal_or_hex) {
    const lexed result = lex("300 0x1F 18446744073709551615");
    ASSERT_FALSE(result.failed);
    EXPECT_EQ(result.tokens[0].value, 300);
    EXPECT_EQ(result.tokens[1].value, 31);
    EXPECT_EQ(result.tokens[2].value, std::numeric_limits<uint64_t>::max());
  }

  TEST_F(command_lexer_tests, bad_numbers_are_errors) {
    EXPECT_TRUE(lex("12abc").failed);
    EXPECT_TRUE(lex("0x").failed);
    EXPECT_TRUE(lex("1.5.2").failed);
    EXPECT_TRUE(lex("1.5abc").failed);
    EXPECT_TRUE(lex("18446744073709551616").failed);
  }

  TEST_F(command_lexer_tests, a_number_with_a_point_is_a_decimal) {
    const lexed result = lex("0.5 128.25 3 7.end");
    ASSERT_FALSE(result.failed);
    const ostd::vector<command_token_type> expected = { CMD_TOKEN_DECIMAL, CMD_TOKEN_DECIMAL, CMD_TOKEN_INTEGER, CMD_TOKEN_INTEGER, CMD_TOKEN_DOT, CMD_TOKEN_NAME, CMD_TOKEN_EOF };
    EXPECT_EQ(types(result), expected);
    EXPECT_DOUBLE_EQ(result.tokens[0].decimal, 0.5);
    EXPECT_DOUBLE_EQ(result.tokens[1].decimal, 128.25);
    EXPECT_EQ(result.tokens[1].text, "128.25");
  }

  TEST_F(command_lexer_tests, struct_enum_and_list_brackets_are_tokens) {
    const lexed result = lex("struct enum [ ]");
    ASSERT_FALSE(result.failed);
    const ostd::vector<command_token_type> expected = { CMD_TOKEN_KW_STRUCT, CMD_TOKEN_KW_ENUM, CMD_TOKEN_LEFT_BRACKET, CMD_TOKEN_RIGHT_BRACKET, CMD_TOKEN_EOF };
    EXPECT_EQ(types(result), expected);
  }

  TEST_F(command_lexer_tests, no_newline_inside_a_list) {
    const lexed result = lex("size = [1,\n  2,\n  3]\n");
    ASSERT_FALSE(result.failed);
    EXPECT_EQ(std::ranges::count(types(result), CMD_TOKEN_NEWLINE), 1);
  }

  TEST_F(command_lexer_tests, string_escapes_are_resolved) {
    const lexed result = lex(R"("a\n\"b\"\\")");
    ASSERT_FALSE(result.failed);
    EXPECT_EQ(result.tokens[0].text, "a\n\"b\"\\");

    EXPECT_TRUE(lex("\"no end").failed);
    EXPECT_TRUE(lex(R"("bad \q")").failed);
  }

  TEST_F(command_lexer_tests, comments_and_blank_lines_leave_one_newline) {
    const lexed result = lex("# heading\n\n  a  # trailing\n\n\n  b\n");
    ASSERT_FALSE(result.failed);
    const ostd::vector<command_token_type> expected = { CMD_TOKEN_NAME, CMD_TOKEN_NEWLINE, CMD_TOKEN_NAME, CMD_TOKEN_NEWLINE, CMD_TOKEN_EOF };
    EXPECT_EQ(types(result), expected);
    EXPECT_EQ(result.tokens[2].span.start.line, 6);
  }

  TEST_F(command_lexer_tests, no_newline_inside_parentheses) {
    const lexed result = lex("f(1,\n  2)\n");
    ASSERT_FALSE(result.failed);
    EXPECT_EQ(std::ranges::count(types(result), CMD_TOKEN_NEWLINE), 1);
  }

  TEST_F(command_lexer_tests, operators_take_the_longest_match) {
    const lexed result = lex("== != <= >= && || |> | = ! < >");
    ASSERT_FALSE(result.failed);
    const ostd::vector<command_token_type> expected = {
      CMD_TOKEN_EQUAL_EQUAL, CMD_TOKEN_BANG_EQUAL, CMD_TOKEN_LESS_EQUAL, CMD_TOKEN_GREATER_EQUAL, CMD_TOKEN_AND_AND, CMD_TOKEN_OR_OR, CMD_TOKEN_PIPE_GREATER,
      CMD_TOKEN_PIPE, CMD_TOKEN_ASSIGN, CMD_TOKEN_BANG, CMD_TOKEN_LESS, CMD_TOKEN_GREATER, CMD_TOKEN_EOF
    };
    EXPECT_EQ(types(result), expected);
    EXPECT_TRUE(lex("a & b").failed);
  }

  TEST_F(command_lexer_tests, space_before_a_token_is_recorded) {
    const lexed result = lex("f(x) g (y)");
    ASSERT_FALSE(result.failed);
    EXPECT_FALSE(result.tokens[1].space_before);
    EXPECT_TRUE(result.tokens[5].space_before);
  }

}  // namespace other
