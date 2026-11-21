/**
 * \file vm/command_language/command_parser.cpp
 **/
#include "vm/command_language/command_parser.hpp"

#include "core/profiler.hpp"

#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/diagnostics/ocmd_errors.hpp"

namespace other {
  namespace {

    std::string describe(const command_token& token) {
      switch (token.type) {
        case CMD_TOKEN_EOF: return "the end of the file";
        case CMD_TOKEN_NEWLINE: return "the end of the line";
        case CMD_TOKEN_STRING: return std::format("\"{}\"", token.text);
        default: return std::format("'{}'", token.text);
      }
    }

    /// 0 for a token that is not a binary operator
    uint8_t precedence(const command_token_type type) {
      switch (type) {
        case CMD_TOKEN_OR_OR: return 1;
        case CMD_TOKEN_AND_AND: return 2;
        case CMD_TOKEN_EQUAL_EQUAL:
        case CMD_TOKEN_BANG_EQUAL: return 3;
        case CMD_TOKEN_LESS:
        case CMD_TOKEN_LESS_EQUAL:
        case CMD_TOKEN_GREATER:
        case CMD_TOKEN_GREATER_EQUAL: return 4;
        case CMD_TOKEN_PLUS:
        case CMD_TOKEN_MINUS: return 5;
        case CMD_TOKEN_STAR:
        case CMD_TOKEN_SLASH:
        case CMD_TOKEN_PERCENT: return 6;
        default: return 0;
      }
    }

    /// what a command's argument can begin with. '(' counts only after a space: 'f(x)' is a call
    bool starts_word(const command_token& token) {
      switch (token.type) {
        case CMD_TOKEN_NAME:
        case CMD_TOKEN_INTEGER:
        case CMD_TOKEN_STRING:
        case CMD_TOKEN_KW_TRUE:
        case CMD_TOKEN_KW_FALSE:
        case CMD_TOKEN_KW_NULL: return true;
        case CMD_TOKEN_LEFT_PAREN: return token.space_before;
        default: return false;
      }
    }

  }  // namespace

  command_file command_parser::parse(diagnostic_engine* diag) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine must not be null");
    OTHER_ASSERT(!tokens.empty() && tokens.back().type == CMD_TOKEN_EOF, "The token list must end in CMD_TOKEN_EOF");
    PROFILE_SECTION("command_parser::parse");

    command_file file;
    bool success = true;

    skip_terminators();
    while (!check(CMD_TOKEN_EOF)) {
      try {
        if (check(CMD_TOKEN_KW_FN)) {
          file.functions.push_back(parse_function());
        } else if (check(CMD_TOKEN_KW_ON)) {
          file.hooks.push_back(parse_hook());
        } else if (check(CMD_TOKEN_KW_USE)) {
          file.uses.push_back(parse_use());
        } else {
          file.statements.push_back(parse_statement());
        }
      } catch (const ocmd_toolchain_error& e) {
        diag->emit({ .severity = VM_DIAGNOSTIC_ERROR, .error_code = e.error, .phase = VM_PHASE_PARSER, .span = e.loc, .final_message = e.msg });
        success = false;
        synchronize();
      }
      skip_terminators();
    }

    file.valid = success;
    return file;
  }

  const command_token& command_parser::peek(size_t ahead) const {
    return tokens[std::min(cursor + ahead, tokens.size() - 1)];
  }

  bool command_parser::match(command_token_type type) {
    if (!check(type)) {
      return false;
    }
    ++cursor;
    return true;
  }

  const command_token& command_parser::expect(command_token_type type, const std::string_view what) {
    if (!check(type)) {
      fail(peek(), std::format("expected {}, found {}", what, describe(peek())));
    }
    return tokens[cursor++];
  }

  void command_parser::fail(const command_token& at, std::string message) const {
    throw ocmd_toolchain_error(PARSE_UNEXPECTED_TOKEN, at.span, message);
  }

  void command_parser::skip_terminators() {
    while (check(CMD_TOKEN_NEWLINE) || check(CMD_TOKEN_SEMICOLON)) {
      ++cursor;
    }
  }

  bool command_parser::at_statement_end() const {
    return check(CMD_TOKEN_NEWLINE) || check(CMD_TOKEN_SEMICOLON) || check(CMD_TOKEN_RIGHT_BRACE) || check(CMD_TOKEN_EOF);
  }

  void command_parser::end_statement() {
    if (check(CMD_TOKEN_PIPE)) {
      fail(peek(), "pipelines ('|') are not supported yet");
    }
    if (check(CMD_TOKEN_PIPE_GREATER)) {
      fail(peek(), "graph edges ('|>') are not supported yet");
    }
    if (!at_statement_end()) {
      fail(peek(), std::format("expected the end of the statement, found {}", describe(peek())));
    }
    if (check(CMD_TOKEN_NEWLINE) || check(CMD_TOKEN_SEMICOLON)) {
      ++cursor;
    }
  }

  /// skips the rest of the statement that failed, and the rest of every block it was inside
  void command_parser::synchronize() {
    size_t depth = block_depth;
    block_depth = 0;
    while (!check(CMD_TOKEN_EOF)) {
      const command_token_type type = peek().type;
      ++cursor;
      if (type == CMD_TOKEN_LEFT_BRACE) {
        ++depth;
      } else if (type == CMD_TOKEN_RIGHT_BRACE) {
        if (depth <= 1) {
          return;
        }
        --depth;
      } else if (depth == 0 && (type == CMD_TOKEN_NEWLINE || type == CMD_TOKEN_SEMICOLON)) {
        return;
      }
    }
  }

  command_function command_parser::parse_function() {
    const command_token& keyword = expect(CMD_TOKEN_KW_FN, "'fn'");
    command_function function = { .name = expect(CMD_TOKEN_NAME, "a function name after 'fn'").text, .span = keyword.span };

    expect(CMD_TOKEN_LEFT_PAREN, "'(' after the function name");
    if (!check(CMD_TOKEN_RIGHT_PAREN)) {
      do {
        function.params.push_back(expect(CMD_TOKEN_NAME, "a parameter name").text);
      } while (match(CMD_TOKEN_COMMA));
    }
    expect(CMD_TOKEN_RIGHT_PAREN, "')' after the parameters");

    /// the return type is read and ignored until the language has types
    if (match(CMD_TOKEN_ARROW)) {
      expect(CMD_TOKEN_NAME, "a type name after '->'");
    }

    function.body = parse_block();
    end_statement();
    return function;
  }

  command_hook command_parser::parse_hook() {
    const command_token& keyword = expect(CMD_TOKEN_KW_ON, "'on'");
    command_hook hook = { .span = keyword.span };
    if (check(CMD_TOKEN_STRING)) {
      hook.engine_event = true;
    } else if (!check(CMD_TOKEN_NAME)) {
      fail(peek(), std::format("expected a hook name or an event name in quotes after 'on', found {}", describe(peek())));
    }
    hook.name = tokens[cursor++].text;

    hook.body = parse_block();
    end_statement();
    return hook;
  }

  command_use command_parser::parse_use() {
    const command_token& keyword = expect(CMD_TOKEN_KW_USE, "'use'");
    const command_use use = { .path = expect(CMD_TOKEN_STRING, "a file path in quotes after 'use'").text, .span = keyword.span };
    end_statement();
    return use;
  }

  ostd::vector<command_stmt> command_parser::parse_block() {
    expect(CMD_TOKEN_LEFT_BRACE, "'{'");
    ++block_depth;

    ostd::vector<command_stmt> statements;
    skip_terminators();
    while (!check(CMD_TOKEN_RIGHT_BRACE)) {
      if (check(CMD_TOKEN_EOF)) {
        fail(peek(), "this block is missing its closing '}'");
      }
      statements.push_back(parse_statement());
      skip_terminators();
    }

    ++cursor;
    --block_depth;
    return statements;
  }

  command_stmt command_parser::parse_statement() {
    const command_token& first = peek();
    command_stmt stmt = { .span = first.span };

    switch (first.type) {
      case CMD_TOKEN_KW_LET:
        ++cursor;
        stmt.kind = CMD_STMT_LET;
        stmt.name = expect(CMD_TOKEN_NAME, "a variable name after 'let'").text;
        expect(CMD_TOKEN_ASSIGN, "'=' after the variable name");
        stmt.exprs.push_back(parse_value());
        break;

      case CMD_TOKEN_KW_IF: stmt = parse_if(); break;

      case CMD_TOKEN_KW_WHILE:
        ++cursor;
        stmt.kind = CMD_STMT_WHILE;
        expect(CMD_TOKEN_LEFT_PAREN, "'(' after 'while'");
        stmt.exprs.push_back(parse_expression());
        expect(CMD_TOKEN_RIGHT_PAREN, "')' after the condition");
        stmt.body = parse_block();
        break;

      case CMD_TOKEN_KW_RETURN:
        ++cursor;
        stmt.kind = CMD_STMT_RETURN;
        if (!at_statement_end()) {
          stmt.exprs.push_back(parse_value());
        }
        break;

      case CMD_TOKEN_KW_BREAK:
        ++cursor;
        stmt.kind = CMD_STMT_BREAK;
        break;

      case CMD_TOKEN_KW_CONTINUE:
        ++cursor;
        stmt.kind = CMD_STMT_CONTINUE;
        break;

      case CMD_TOKEN_KW_FN:
      case CMD_TOKEN_KW_ON:
      case CMD_TOKEN_KW_USE: fail(first, std::format("'{}' is only allowed outside every block", first.text));

      case CMD_TOKEN_KW_RESERVED: fail(first, std::format("'{}' is reserved for a later version of the language", first.text));

      case CMD_TOKEN_NAME:
        if (peek(1).type != CMD_TOKEN_ASSIGN) {
          return parse_commands();
        }
        stmt.kind = CMD_STMT_ASSIGN;
        stmt.name = first.text;
        cursor += 2;
        stmt.exprs.push_back(parse_value());
        break;

      default: fail(first, std::format("a statement cannot start with {}", describe(first)));
    }

    end_statement();
    return stmt;
  }

  command_stmt command_parser::parse_if() {
    const command_token& keyword = expect(CMD_TOKEN_KW_IF, "'if'");
    command_stmt stmt = { .kind = CMD_STMT_IF, .span = keyword.span };

    expect(CMD_TOKEN_LEFT_PAREN, "'(' after 'if'");
    stmt.exprs.push_back(parse_expression());
    expect(CMD_TOKEN_RIGHT_PAREN, "')' after the condition");
    stmt.body = parse_block();

    /// 'else' may sit on the line after the closing brace
    const size_t after_block = cursor;
    while (match(CMD_TOKEN_NEWLINE)) {
    }
    if (!match(CMD_TOKEN_KW_ELSE)) {
      cursor = after_block;
      return stmt;
    }

    if (check(CMD_TOKEN_KW_IF)) {
      stmt.else_body.push_back(parse_if());
    } else {
      stmt.else_body = parse_block();
    }
    return stmt;
  }

  command_stmt command_parser::parse_commands() {
    command_stmt stmt = { .kind = CMD_STMT_COMMANDS, .span = peek().span };
    stmt.exprs.push_back(parse_command());

    while (check(CMD_TOKEN_AND_AND) || check(CMD_TOKEN_OR_OR)) {
      const command_token& join = tokens[cursor++];
      /// the next command may start on the following line
      while (match(CMD_TOKEN_NEWLINE)) {
      }
      if (!check(CMD_TOKEN_NAME)) {
        fail(peek(), std::format("expected a command after '{}', found {}", join.text, describe(peek())));
      }
      stmt.joins.push_back(join.type);
      stmt.exprs.push_back(parse_command());
    }

    end_statement();
    return stmt;
  }

  command_expr command_parser::parse_command() {
    const command_token& head = expect(CMD_TOKEN_NAME, "a command");
    command_expr call = { .kind = CMD_EXPR_CALL, .span = head.span, .text = head.text };

    const bool has_member = (check(CMD_TOKEN_DOT) || check(CMD_TOKEN_COLON)) && !peek().space_before && peek(1).type == CMD_TOKEN_NAME && !peek(1).space_before;
    if (has_member) {
      call.member = peek(1).text;
      cursor += 2;
    }

    if (check(CMD_TOKEN_LEFT_PAREN) && !peek().space_before) {
      parse_call_arguments(call);
      return call;
    }

    call.word_form = true;
    while (!at_statement_end() && !check(CMD_TOKEN_AND_AND) && !check(CMD_TOKEN_OR_OR) && !check(CMD_TOKEN_PIPE) && !check(CMD_TOKEN_PIPE_GREATER)) {
      call.operands.push_back(parse_word());
      call.span.end = call.operands.back().span.end;
    }
    return call;
  }

  command_expr command_parser::parse_word() {
    if (!starts_word(peek()) && !check(CMD_TOKEN_LEFT_PAREN)) {
      fail(peek(), std::format("expected a value, a name or '( expression )', found {}", describe(peek())));
    }
    return parse_primary();
  }

  /// the right side of 'let', '=' and 'return': a command when it reads as one, else an expression
  command_expr command_parser::parse_value() {
    return starts_command() ? parse_command() : parse_expression();
  }

  bool command_parser::starts_command() const {
    if (!check(CMD_TOKEN_NAME)) {
      return false;
    }
    const bool has_member = (peek(1).type == CMD_TOKEN_DOT || peek(1).type == CMD_TOKEN_COLON) && !peek(1).space_before && peek(2).type == CMD_TOKEN_NAME && !peek(2).space_before;
    const command_token& after_head = peek(has_member ? 3 : 1);
    /// 'driver.verb' alone is a command with no arguments; a bare name alone is a variable
    return starts_word(after_head) || (has_member && after_head.type != CMD_TOKEN_LEFT_PAREN);
  }

  command_expr command_parser::parse_expression(uint8_t min_precedence) {
    command_expr left = parse_unary();
    while (true) {
      const command_token& op = peek();
      const uint8_t level = precedence(op.type);
      if (level == 0 || level < min_precedence) {
        return left;
      }
      ++cursor;

      command_expr right = parse_expression(static_cast<uint8_t>(level + 1));
      command_expr binary = { .kind = CMD_EXPR_BINARY, .span = { left.span.start, right.span.end }, .op = op.type };
      binary.operands.push_back(std::move(left));
      binary.operands.push_back(std::move(right));
      left = std::move(binary);
    }
  }

  command_expr command_parser::parse_unary() {
    if (!check(CMD_TOKEN_BANG) && !check(CMD_TOKEN_MINUS)) {
      return parse_primary();
    }
    const command_token& op = tokens[cursor++];
    command_expr unary = { .kind = CMD_EXPR_UNARY, .span = op.span, .op = op.type };
    unary.operands.push_back(parse_unary());
    unary.span.end = unary.operands.back().span.end;
    return unary;
  }

  command_expr command_parser::parse_primary() {
    const command_token& token = tokens[std::min(cursor, tokens.size() - 1)];
    switch (token.type) {
      case CMD_TOKEN_INTEGER:
        ++cursor;
        return { .kind = CMD_EXPR_INTEGER, .span = token.span, .integer = token.value };
      case CMD_TOKEN_STRING:
        ++cursor;
        return { .kind = CMD_EXPR_STRING, .span = token.span, .text = token.text };
      case CMD_TOKEN_KW_TRUE:
      case CMD_TOKEN_KW_FALSE:
        ++cursor;
        return { .kind = CMD_EXPR_BOOL, .span = token.span, .integer = token.type == CMD_TOKEN_KW_TRUE ? uint64_t{ 1 } : uint64_t{ 0 } };
      case CMD_TOKEN_KW_NULL:
        ++cursor;
        return { .kind = CMD_EXPR_NULL, .span = token.span };

      case CMD_TOKEN_LEFT_PAREN: {
        ++cursor;
        command_expr inner = parse_expression();
        expect(CMD_TOKEN_RIGHT_PAREN, "')'");
        return inner;
      }

      case CMD_TOKEN_NAME: {
        ++cursor;
        command_expr name = { .kind = CMD_EXPR_NAME, .span = token.span, .text = token.text };
        const bool has_member = (check(CMD_TOKEN_DOT) || check(CMD_TOKEN_COLON)) && !peek().space_before && peek(1).type == CMD_TOKEN_NAME && !peek(1).space_before;
        if (has_member) {
          name.member = peek(1).text;
          cursor += 2;
        }

        if (check(CMD_TOKEN_LEFT_PAREN) && !peek().space_before) {
          name.kind = CMD_EXPR_CALL;
          parse_call_arguments(name);
        } else if (has_member) {
          fail(token, std::format("'{}.{}' needs '( )' here; reading a field is not supported yet", name.text, name.member));
        }
        return name;
      }

      case CMD_TOKEN_KW_RESERVED: fail(token, std::format("'{}' is reserved for a later version of the language", token.text));
      default: fail(token, std::format("expected a value, found {}", describe(token)));
    }
  }

  void command_parser::parse_call_arguments(command_expr& call) {
    expect(CMD_TOKEN_LEFT_PAREN, "'('");
    if (!check(CMD_TOKEN_RIGHT_PAREN)) {
      do {
        call.operands.push_back(parse_expression());
      } while (match(CMD_TOKEN_COMMA));
    }
    call.span.end = expect(CMD_TOKEN_RIGHT_PAREN, "')' after the arguments").span.end;
  }

}  // namespace other
