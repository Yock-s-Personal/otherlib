/**
 * \file vm/command_language/command_parser.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_PARSER_HPP
#define OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_PARSER_HPP

#include <span>

#include "vm/command_language/command_ast.hpp"

namespace other {

  class diagnostic_engine;

  struct command_parse_options {
    /// a file of declarations: it holds declarations, structs, enums and 'use', and nothing
    ///  that runs. a declaration in it may be in its kind's short form
    bool declarations_only = false;
  };

  class command_parser {
   public:
    /// tokens must end in CMD_TOKEN_EOF, as command_lexer leaves them
    explicit command_parser(const std::span<const command_token> tokens, const command_parse_options options = {})
        : tokens(tokens), options(options) {}

    /// reports every statement it cannot read and carries on; valid is false if there was one
    command_file parse(diagnostic_engine* diag);

   private:
    const command_token& peek(size_t ahead = 0) const;
    bool check(command_token_type type) const { return peek().type == type; }
    bool match(command_token_type type);
    const command_token& expect(command_token_type type, std::string_view what);
    [[noreturn]] void fail(const command_token& at, std::string message) const;

    void skip_terminators();
    bool at_statement_end() const;
    void end_statement();
    void synchronize();

    command_function parse_function();
    command_hook parse_hook();
    command_use parse_use();
    command_enum parse_enum();
    command_struct parse_struct();
    command_declaration parse_declaration(bool short_forms);
    void parse_declaration_body(command_declaration& declaration);
    command_value parse_declared_value();
    bool starts_declaration() const;
    void skip_separators();
    ostd::vector<command_stmt> parse_block();
    command_stmt parse_statement();
    command_stmt parse_if();
    command_stmt parse_commands();
    command_expr parse_command();
    command_expr parse_word();
    command_expr parse_value();
    bool starts_command() const;

    command_expr parse_expression(uint8_t min_precedence = 1);
    command_expr parse_unary();
    command_expr parse_primary();
    void parse_call_arguments(command_expr& call);

    std::span<const command_token> tokens;
    command_parse_options options;
    size_t cursor = 0;
    size_t block_depth = 0;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_PARSER_HPP
