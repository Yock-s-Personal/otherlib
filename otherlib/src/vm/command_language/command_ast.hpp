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

  enum command_value_kind : uint8_t {
    CMD_VALUE_INTEGER,
    CMD_VALUE_DECIMAL,
    CMD_VALUE_BOOL,
    /// in quotes
    CMD_VALUE_TEXT,
    /// a bare word: a value of an enum, another declaration's name, or a name the engine knows
    CMD_VALUE_NAME,
    CMD_VALUE_LIST,
    /// 'null': the field is left unsaid, and nothing is written for it. as a struct's default
    ///  it makes a field one that need not be given
    CMD_VALUE_NULL,
  };

  /// what a declaration gives a field. a value is written out and never computed
  struct command_value {
    command_value_kind kind = CMD_VALUE_INTEGER;
    source_span span = {};
    /// INTEGER, and BOOL as 0 or 1
    int64_t integer = 0;
    /// DECIMAL, and INTEGER as the same number
    double decimal = 0.0;
    /// TEXT and NAME
    std::string text;
    /// LIST
    ostd::vector<command_value> items;
  };

  struct command_enum {
    std::string name;
    source_span span = {};
    ostd::vector<std::string> values;
  };

  struct command_struct_field {
    /// int, float, bool, text, name, asset, any, or the name of an enum or a struct
    std::string type;
    /// written 'type[]'
    bool list = false;
    std::string name;
    source_span span = {};
    /// what the field is when a declaration does not say. without one it has to be said
    opt<command_value> initial;
  };

  /// the shape of a kind of thing: what a declaration of it can say
  struct command_struct {
    std::string name;
    source_span span = {};
    /// 'struct name(a, b)': the fields a declaration may give in order with no names
    ostd::vector<std::string> short_form;
    /// 'struct name names a, b': a declaration of it is named after a declaration of one of
    ///  these kinds, which it is about
    ostd::vector<std::string> names;
    ostd::vector<command_struct_field> fields;
    /// 'graph field': the list whose declarations a body can join with |>
    std::string graph;
  };

  struct command_field_value {
    std::string name;
    source_span span = {};
    command_value value;
  };

  struct command_edge {
    std::string from;
    std::string to;
    source_span span = {};
  };

  /// 'kind name { ... }', or 'kind name value value' where the kind has a short form
  struct command_declaration {
    std::string kind;
    /// empty when it was given none
    std::string name;
    source_span span = {};
    source_span name_span = {};
    bool short_form = false;
    /// short form: the values, in the order they were written
    ostd::vector<command_value> positional;
    ostd::vector<command_field_value> fields;
    /// the declarations written inside this one
    ostd::vector<command_declaration> parts;
    ostd::vector<command_edge> edges;
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
    /// LET given a declaration: 'let x = kind { ... }'. none or one
    ostd::vector<command_declaration> declared;
    /// LET given a text that names a file of declarations: the build's name for that file.
    ///  set when the files of a build are read, not by the parser
    std::string included;
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
    ostd::vector<command_enum> enums;
    ostd::vector<command_struct> structs;
    /// what is declared outside any block and bound to no variable, in order
    ostd::vector<command_declaration> declarations;
    ostd::vector<command_function> functions;
    ostd::vector<command_hook> hooks;
    /// what is written outside any function, in order
    ostd::vector<command_stmt> statements;
    bool valid = false;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_AST_HPP
