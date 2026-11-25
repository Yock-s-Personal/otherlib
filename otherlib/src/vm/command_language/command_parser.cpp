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
        case CMD_TOKEN_DECIMAL:
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
        if (check(CMD_TOKEN_KW_USE)) {
          file.uses.push_back(parse_use());
        } else if (check(CMD_TOKEN_KW_ENUM)) {
          file.enums.push_back(parse_enum());
          end_statement();
        } else if (check(CMD_TOKEN_KW_STRUCT)) {
          file.structs.push_back(parse_struct());
          end_statement();
        } else if (options.declarations_only) {
          if (!check(CMD_TOKEN_NAME)) {
            fail(peek(), std::format("a file of declarations holds declarations, structs, enums and 'use'; found {}", describe(peek())));
          }
          file.declarations.push_back(parse_declaration(true));
          end_statement();
        } else if (check(CMD_TOKEN_KW_FN)) {
          file.functions.push_back(parse_function());
        } else if (check(CMD_TOKEN_KW_ON)) {
          file.hooks.push_back(parse_hook());
        } else if (starts_declaration()) {
          file.declarations.push_back(parse_declaration(false));
          end_statement();
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

  void command_parser::skip_separators() {
    while (check(CMD_TOKEN_NEWLINE) || check(CMD_TOKEN_SEMICOLON) || check(CMD_TOKEN_COMMA)) {
      ++cursor;
    }
  }

  /// 'kind name {' or 'kind {'. a command cannot be followed by a brace, so where a statement
  ///  can stand these can only be a declaration
  bool command_parser::starts_declaration() const {
    if (!check(CMD_TOKEN_NAME)) {
      return false;
    }
    return peek(1).type == CMD_TOKEN_LEFT_BRACE || (peek(1).type == CMD_TOKEN_NAME && peek(2).type == CMD_TOKEN_LEFT_BRACE);
  }

  command_enum command_parser::parse_enum() {
    const command_token& keyword = expect(CMD_TOKEN_KW_ENUM, "'enum'");
    command_enum result = { .name = expect(CMD_TOKEN_NAME, "a name after 'enum'").text, .span = keyword.span };

    expect(CMD_TOKEN_LEFT_BRACE, "'{' after the enum's name");
    ++block_depth;
    skip_separators();
    while (!check(CMD_TOKEN_RIGHT_BRACE)) {
      if (check(CMD_TOKEN_EOF)) {
        fail(peek(), std::format("enum '{}' is missing its closing '}}'", result.name));
      }
      result.values.push_back(expect(CMD_TOKEN_NAME, "the name of a value").text);
      skip_separators();
    }
    result.span.end = tokens[cursor].span.end;
    ++cursor;
    --block_depth;
    return result;
  }

  command_struct command_parser::parse_struct() {
    const command_token& keyword = expect(CMD_TOKEN_KW_STRUCT, "'struct'");
    command_struct result = { .name = expect(CMD_TOKEN_NAME, "a name after 'struct'").text, .span = keyword.span };

    if (check(CMD_TOKEN_LEFT_PAREN) && !peek().space_before) {
      ++cursor;
      do {
        result.short_form.push_back(expect(CMD_TOKEN_NAME, "the name of a field its short form gives").text);
      } while (match(CMD_TOKEN_COMMA));
      expect(CMD_TOKEN_RIGHT_PAREN, "')' after the short form's fields");
    }
    if (check(CMD_TOKEN_NAME) && peek().text == "names") {
      ++cursor;
      do {
        result.names.push_back(expect(CMD_TOKEN_NAME, "the kind of thing it is named after").text);
      } while (match(CMD_TOKEN_COMMA));
    }

    expect(CMD_TOKEN_LEFT_BRACE, "'{' before the struct's fields");
    ++block_depth;
    skip_separators();
    while (!check(CMD_TOKEN_RIGHT_BRACE)) {
      if (check(CMD_TOKEN_EOF)) {
        fail(peek(), std::format("struct '{}' is missing its closing '}}'", result.name));
      }

      const command_token& first = expect(CMD_TOKEN_NAME, "a field's type");
      /// 'graph field' says which of its lists a body can join with |>
      const command_token_type after = peek(1).type;
      const bool line_ends = after == CMD_TOKEN_NEWLINE || after == CMD_TOKEN_SEMICOLON || after == CMD_TOKEN_COMMA || after == CMD_TOKEN_RIGHT_BRACE;
      if (first.text == "graph" && check(CMD_TOKEN_NAME) && line_ends) {
        if (!result.graph.empty()) {
          fail(peek(), std::format("struct '{}' already has '{}' as its graph", result.name, result.graph));
        }
        result.graph = tokens[cursor++].text;
        skip_separators();
        continue;
      }

      command_struct_field field = { .type = first.text, .span = first.span };
      if (match(CMD_TOKEN_LEFT_BRACKET)) {
        expect(CMD_TOKEN_RIGHT_BRACKET, "']': a list's type is written 'type[]'");
        field.list = true;
      }
      field.name = expect(CMD_TOKEN_NAME, "the field's name after its type").text;
      if (match(CMD_TOKEN_ASSIGN)) {
        field.initial = parse_declared_value();
      }
      result.fields.push_back(std::move(field));
      skip_separators();
    }
    result.span.end = tokens[cursor].span.end;
    ++cursor;
    --block_depth;
    return result;
  }

  command_declaration command_parser::parse_declaration(const bool short_forms) {
    const command_token& kind = expect(CMD_TOKEN_NAME, "the kind of thing declared");
    command_declaration declaration = { .kind = kind.text, .span = kind.span, .name_span = kind.span };
    if (check(CMD_TOKEN_NAME)) {
      declaration.name = peek().text;
      declaration.name_span = peek().span;
      ++cursor;
    }

    if (check(CMD_TOKEN_LEFT_BRACE)) {
      parse_declaration_body(declaration);
      return declaration;
    }
    if (!short_forms || declaration.name.empty()) {
      fail(peek(), std::format("expected '{{' after '{}{}{}', found {}", declaration.kind, declaration.name.empty() ? "" : " ", declaration.name, describe(peek())));
    }

    declaration.short_form = true;
    while (!at_statement_end()) {
      declaration.positional.push_back(parse_declared_value());
      declaration.span.end = declaration.positional.back().span.end;
    }
    return declaration;
  }

  void command_parser::parse_declaration_body(command_declaration& declaration) {
    expect(CMD_TOKEN_LEFT_BRACE, "'{'");
    ++block_depth;
    skip_separators();
    while (!check(CMD_TOKEN_RIGHT_BRACE)) {
      if (check(CMD_TOKEN_EOF)) {
        fail(peek(), std::format("'{}{}{}' is missing its closing '}}'", declaration.kind, declaration.name.empty() ? "" : " ", declaration.name));
      }
      if (!check(CMD_TOKEN_NAME)) {
        fail(peek(), std::format("expected a field, a declaration or an edge inside '{}', found {}", declaration.kind, describe(peek())));
      }

      const command_token_type after = peek(1).type;
      if (after == CMD_TOKEN_ASSIGN) {
        command_field_value field = { .name = peek().text, .span = peek().span };
        cursor += 2;
        field.value = parse_declared_value();
        declaration.fields.push_back(std::move(field));
      } else if (after == CMD_TOKEN_PIPE_GREATER) {
        /// 'a |> b |> c' is two edges
        std::string from = tokens[cursor++].text;
        while (check(CMD_TOKEN_PIPE_GREATER)) {
          const source_span span = peek().span;
          ++cursor;
          std::string to = expect(CMD_TOKEN_NAME, "a name after '|>'").text;
          declaration.edges.push_back({ .from = from, .to = to, .span = span });
          from = std::move(to);
        }
      } else {
        declaration.parts.push_back(parse_declaration(true));
      }
      skip_separators();
    }
    declaration.span.end = tokens[cursor].span.end;
    ++cursor;
    --block_depth;
  }

  command_value command_parser::parse_declared_value() {
    const command_token& token = peek();
    switch (token.type) {
      case CMD_TOKEN_INTEGER:
      case CMD_TOKEN_DECIMAL:
      case CMD_TOKEN_MINUS: {
        const bool negative = token.type == CMD_TOKEN_MINUS;
        if (negative) {
          ++cursor;
          if (!check(CMD_TOKEN_INTEGER) && !check(CMD_TOKEN_DECIMAL)) {
            fail(peek(), std::format("expected a number after '-', found {}", describe(peek())));
          }
        }
        const command_token& number = tokens[cursor++];
        command_value value = { .span = { token.span.start, number.span.end } };
        if (number.type == CMD_TOKEN_DECIMAL) {
          value.kind = CMD_VALUE_DECIMAL;
          value.decimal = negative ? -number.decimal : number.decimal;
          return value;
        }
        /// one more below zero than above it fits
        constexpr uint64_t kMost = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
        if (number.value > kMost + (negative ? 1u : 0u)) {
          fail(number, std::format("'{}{}' does not fit in a signed 64-bit number", negative ? "-" : "", number.text));
        }
        value.kind = CMD_VALUE_INTEGER;
        value.integer = negative ? static_cast<int64_t>(0u - number.value) : static_cast<int64_t>(number.value);
        value.decimal = static_cast<double>(value.integer);
        return value;
      }

      case CMD_TOKEN_STRING:
        ++cursor;
        return { .kind = CMD_VALUE_TEXT, .span = token.span, .text = token.text };

      case CMD_TOKEN_KW_TRUE:
      case CMD_TOKEN_KW_FALSE:
        ++cursor;
        return { .kind = CMD_VALUE_BOOL, .span = token.span, .integer = token.type == CMD_TOKEN_KW_TRUE ? 1 : 0 };

      case CMD_TOKEN_KW_NULL:
        ++cursor;
        return { .kind = CMD_VALUE_NULL, .span = token.span };

      case CMD_TOKEN_NAME:
        ++cursor;
        return { .kind = CMD_VALUE_NAME, .span = token.span, .text = token.text };

      case CMD_TOKEN_LEFT_BRACKET: {
        ++cursor;
        command_value list = { .kind = CMD_VALUE_LIST, .span = token.span };
        while (!check(CMD_TOKEN_RIGHT_BRACKET)) {
          if (check(CMD_TOKEN_EOF)) {
            fail(token, "this list is missing its closing ']'");
          }
          list.items.push_back(parse_declared_value());
          /// null is a field left unsaid, and a list has no fields
          if (list.items.back().kind == CMD_VALUE_NULL) {
            fail(tokens[cursor - 1], "a list cannot hold null; leave the item out");
          }
          if (!match(CMD_TOKEN_COMMA) && !check(CMD_TOKEN_RIGHT_BRACKET)) {
            fail(peek(), std::format("expected ',' or ']' in the list, found {}", describe(peek())));
          }
        }
        list.span.end = tokens[cursor].span.end;
        ++cursor;
        return list;
      }

      default: fail(token, std::format("expected a value, found {}", describe(token)));
    }
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
        if (starts_declaration()) {
          /// a declaration is the thing it declares, and a variable can be it
          if (block_depth > 0) {
            fail(peek(), "a variable can be given a declaration only outside every block for now");
          }
          stmt.declared.push_back(parse_declaration(false));
        } else {
          stmt.exprs.push_back(parse_value());
        }
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
      case CMD_TOKEN_KW_USE:
      case CMD_TOKEN_KW_STRUCT:
      case CMD_TOKEN_KW_ENUM: fail(first, std::format("'{}' is only allowed outside every block", first.text));

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

      case CMD_TOKEN_DECIMAL: fail(token, std::format("'{}': a number with a point can be given in a declaration; what runs has whole numbers only for now", token.text));

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
