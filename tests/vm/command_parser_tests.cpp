/**
 * \file tests/vm/command_parser_tests.cpp
 **/
#include "vm/command_language/command_lexer.hpp"
#include "vm/command_language/command_parser.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"

#include "other_test.hpp"

namespace other {

  class command_parser_tests : public other_test {
   protected:
    static command_file parse(const std::string_view text) {
      diagnostic_engine diag;
      command_lexer lexer{ text };
      const ostd::vector<command_token> tokens = lexer.tokenize(&diag);
      if (lexer.failed()) {
        return {};
      }
      return command_parser{ tokens }.parse(&diag);
    }
  };

  TEST_F(command_parser_tests, a_command_keeps_its_words_in_order) {
    const command_file file = parse("asset load_asset \"models/ship.obj\" 3\n");
    ASSERT_TRUE(file.valid);
    ASSERT_EQ(file.statements.size(), 1);

    const command_stmt& stmt = file.statements[0];
    ASSERT_EQ(stmt.kind, CMD_STMT_COMMANDS);
    const command_expr& call = stmt.exprs[0];
    EXPECT_EQ(call.kind, CMD_EXPR_CALL);
    EXPECT_TRUE(call.word_form);
    EXPECT_EQ(call.text, "asset");
    ASSERT_EQ(call.operands.size(), 3);
    EXPECT_EQ(call.operands[0].kind, CMD_EXPR_NAME);
    EXPECT_EQ(call.operands[1].kind, CMD_EXPR_STRING);
    EXPECT_EQ(call.operands[2].integer, 3);
  }

  TEST_F(command_parser_tests, the_three_spellings_of_a_member_command) {
    const command_file file = parse("asset.load_asset \"a\"\nasset:load_asset \"a\"\nasset.load_asset(\"a\")\n");
    ASSERT_TRUE(file.valid);
    ASSERT_EQ(file.statements.size(), 3);
    for (const command_stmt& stmt : file.statements) {
      EXPECT_EQ(stmt.exprs[0].text, "asset");
      EXPECT_EQ(stmt.exprs[0].member, "load_asset");
      EXPECT_EQ(stmt.exprs[0].operands.size(), 1);
    }
    EXPECT_TRUE(file.statements[0].exprs[0].word_form);
    EXPECT_FALSE(file.statements[2].exprs[0].word_form);
  }

  TEST_F(command_parser_tests, a_parenthesis_after_a_space_is_a_word) {
    const command_file file = parse("f (1 + 2) 3\ng(1 + 2, 3)\n");
    ASSERT_TRUE(file.valid);
    EXPECT_TRUE(file.statements[0].exprs[0].word_form);
    EXPECT_EQ(file.statements[0].exprs[0].operands[0].kind, CMD_EXPR_BINARY);
    EXPECT_EQ(file.statements[0].exprs[0].operands.size(), 2);
    EXPECT_FALSE(file.statements[1].exprs[0].word_form);
    EXPECT_EQ(file.statements[1].exprs[0].operands.size(), 2);
  }

  TEST_F(command_parser_tests, operators_bind_as_in_c) {
    const command_file file = parse("let x = 1 + 2 * 3 == 7 && !done\n");
    ASSERT_TRUE(file.valid);
    const command_expr& root = file.statements[0].exprs[0];
    ASSERT_EQ(root.op, CMD_TOKEN_AND_AND);
    ASSERT_EQ(root.operands[0].op, CMD_TOKEN_EQUAL_EQUAL);
    const command_expr& sum = root.operands[0].operands[0];
    ASSERT_EQ(sum.op, CMD_TOKEN_PLUS);
    EXPECT_EQ(sum.operands[1].op, CMD_TOKEN_STAR);
    EXPECT_EQ(root.operands[1].kind, CMD_EXPR_UNARY);
  }

  TEST_F(command_parser_tests, the_right_side_of_let_can_be_a_command) {
    const command_file file = parse("let id = scene load_scene \"main\"\nlet copy = id\nlet n = count()\nlet up = project.loaded\n");
    ASSERT_TRUE(file.valid);
    EXPECT_EQ(file.statements[0].exprs[0].kind, CMD_EXPR_CALL);
    EXPECT_TRUE(file.statements[0].exprs[0].word_form);
    EXPECT_EQ(file.statements[1].exprs[0].kind, CMD_EXPR_NAME);
    EXPECT_EQ(file.statements[2].exprs[0].kind, CMD_EXPR_CALL);
    EXPECT_EQ(file.statements[3].exprs[0].member, "loaded");
  }

  TEST_F(command_parser_tests, commands_chain_on_and_and_or) {
    const command_file file = parse("project load \"a\" && core log \"ok\" 2 ||\n  core log \"failed\" 4\n");
    ASSERT_TRUE(file.valid);
    ASSERT_EQ(file.statements.size(), 1);
    const command_stmt& stmt = file.statements[0];
    ASSERT_EQ(stmt.exprs.size(), 3);
    const ostd::vector<command_token_type> joins = { CMD_TOKEN_AND_AND, CMD_TOKEN_OR_OR };
    EXPECT_EQ(stmt.joins, joins);
  }

  TEST_F(command_parser_tests, if_else_and_while_nest) {
    const command_file file = parse("if (a) { b }\nelse if (c) { d } else { e }\nwhile (i < 3) { i = i + 1; if (i == 2) { break } }\n");
    ASSERT_TRUE(file.valid);
    ASSERT_EQ(file.statements.size(), 2);

    const command_stmt& first = file.statements[0];
    ASSERT_EQ(first.kind, CMD_STMT_IF);
    ASSERT_EQ(first.else_body.size(), 1);
    EXPECT_EQ(first.else_body[0].kind, CMD_STMT_IF);
    EXPECT_EQ(first.else_body[0].else_body.size(), 1);

    const command_stmt& loop = file.statements[1];
    ASSERT_EQ(loop.kind, CMD_STMT_WHILE);
    ASSERT_EQ(loop.body.size(), 2);
    EXPECT_EQ(loop.body[0].kind, CMD_STMT_ASSIGN);
    EXPECT_EQ(loop.body[1].body[0].kind, CMD_STMT_BREAK);
  }

  TEST_F(command_parser_tests, functions_hooks_and_uses_are_collected) {
    const command_file file = parse("use \"lib/math.ocmd\"\nfn add(a, b) -> int { return a + b }\non pre_load { }\non \"asset.loaded\" { }\nadd 1 2\n");
    ASSERT_TRUE(file.valid);
    ASSERT_EQ(file.uses.size(), 1);
    EXPECT_EQ(file.uses[0].path, "lib/math.ocmd");
    ASSERT_EQ(file.functions.size(), 1);
    EXPECT_EQ(file.functions[0].params.size(), 2);
    EXPECT_EQ(file.functions[0].body[0].kind, CMD_STMT_RETURN);
    ASSERT_EQ(file.hooks.size(), 2);
    EXPECT_FALSE(file.hooks[0].engine_event);
    EXPECT_TRUE(file.hooks[1].engine_event);
    EXPECT_EQ(file.statements.size(), 1);
  }

  TEST_F(command_parser_tests, a_bad_statement_does_not_hide_the_next_one) {
    const command_file file = parse("let = 5\ncore log \"still read\" 2\nif (x { }\ncore log \"and this\" 2\n");
    EXPECT_FALSE(file.valid);
    EXPECT_EQ(file.statements.size(), 2);
  }

  TEST_F(command_parser_tests, what_is_not_supported_yet_is_refused) {
    EXPECT_FALSE(parse("scene find_object \"a\" | core log_u64 2\n").valid);
    EXPECT_FALSE(parse("asset load_asset \"a\" |> scene load_scene \"b\"\n").valid);
    EXPECT_FALSE(parse("error \"boom\"\n").valid);
    EXPECT_FALSE(parse("if (true) { fn inner() { } }\n").valid);
    EXPECT_FALSE(parse("let x = scene.name + 1\n").valid);
  }

}  // namespace other
