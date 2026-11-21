/**
 * \file vm/command_language/command_ast.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_AST_HPP
#define OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_AST_HPP

#include "data-structures/std_container.hpp"

#include "vm/command_language/command_token.hpp"

namespace other {

  enum command_expr_kind : uint8_t {
    CMD_EXPR_INTEGER,
    CMD_EXPR_STRING,
    CMD_EXPR_BOOL,
    CMD_EXPR_NULL,
    CMD_EXPR_NAME,
    CMD_EXPR_UNARY,
    CMD_EXPR_BINARY,
    CMD_EXPR_CALL,
  };

  struct command_expr {
    command_expr_kind kind = CMD_EXPR_NULL;
    source_span span = {};
    /// INTEGER and BOOL
    uint64_t integer = 0;
    /// STRING: the value. NAME: the name. CALL: the function, or the driver when member is set
    std::string text;
    /// CALL written 'driver.verb' or 'driver:verb'
    std::string member;
    /// CALL written as a command, 'head word word'. whether its first word is a verb is
    ///  decided when lowering, by looking the word up on the driver
    bool word_form = false;
    /// UNARY and BINARY
    command_token_type op = CMD_TOKEN_INVALID;
    /// UNARY: one. BINARY: two. CALL: the arguments
    ostd::vector<command_expr> operands;
  };

  enum command_stmt_kind : uint8_t {
    CMD_STMT_LET,
    CMD_STMT_ASSIGN,
    /// one or more commands joined by && and ||
    CMD_STMT_COMMANDS,
    CMD_STMT_IF,
    CMD_STMT_WHILE,
    CMD_STMT_RETURN,
    CMD_STMT_BREAK,
    CMD_STMT_CONTINUE,
  };

  struct command_stmt {
    command_stmt_kind kind = CMD_STMT_COMMANDS;
    source_span span = {};
    /// LET and ASSIGN: the variable
    std::string name;
    /// LET, ASSIGN: the value. IF, WHILE: the condition. RETURN: the value, if any.
    ///  COMMANDS: each command, a CALL
    ostd::vector<command_expr> exprs;
    /// COMMANDS: the operator in front of exprs[i + 1]
    ostd::vector<command_token_type> joins;
    ostd::vector<command_stmt> body;
    ostd::vector<command_stmt> else_body;
  };

  struct command_function {
    std::string name;
    source_span span = {};
    ostd::vector<std::string> params;
    ostd::vector<command_stmt> body;
  };

  struct command_hook {
    /// a VM hook such as pre_load, or the name of an engine event
    std::string name;
    bool engine_event = false;
    source_span span = {};
    ostd::vector<command_stmt> body;
  };

  struct command_use {
    std::string path;
    source_span span = {};
  };

  struct command_file {
    ostd::vector<command_use> uses;
    ostd::vector<command_function> functions;
    ostd::vector<command_hook> hooks;
    /// what is written outside any function, in order
    ostd::vector<command_stmt> statements;
    bool valid = false;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_AST_HPP
