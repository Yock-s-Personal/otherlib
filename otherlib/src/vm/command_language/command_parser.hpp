/**
 * \file vm/command_language/command_parser.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_PARSER_HPP
#define OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_PARSER_HPP

#include <span>

#include "vm/command_language/command_ast.hpp"

namespace other {

  class diagnostic_engine;

  class command_parser {
   public:
    /// tokens must end in CMD_TOKEN_EOF, as command_lexer leaves them
    explicit command_parser(const std::span<const command_token> tokens)
        : tokens(tokens) {}

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
    size_t cursor = 0;
    size_t block_depth = 0;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_PARSER_HPP
