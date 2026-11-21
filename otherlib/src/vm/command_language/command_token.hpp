/**
 * \file vm/command_language/command_token.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_TOKEN_HPP
#define OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_TOKEN_HPP

#include "vm/diagnostics/vm_diagnostic.hpp"

namespace other {

  enum command_token_type : uint8_t {
    CMD_TOKEN_INVALID,
    CMD_TOKEN_EOF,
    /// ends a statement; never produced inside parentheses
    CMD_TOKEN_NEWLINE,
    CMD_TOKEN_NAME,
    CMD_TOKEN_INTEGER,
    CMD_TOKEN_STRING,

    CMD_TOKEN_KW_LET,
    CMD_TOKEN_KW_FN,
    CMD_TOKEN_KW_RETURN,
    CMD_TOKEN_KW_IF,
    CMD_TOKEN_KW_ELSE,
    CMD_TOKEN_KW_WHILE,
    CMD_TOKEN_KW_BREAK,
    CMD_TOKEN_KW_CONTINUE,
    CMD_TOKEN_KW_ON,
    CMD_TOKEN_KW_USE,
    CMD_TOKEN_KW_TRUE,
    CMD_TOKEN_KW_FALSE,
    CMD_TOKEN_KW_NULL,
    /// words held back for later versions: error, throw, for
    CMD_TOKEN_KW_RESERVED,

    CMD_TOKEN_LEFT_PAREN,
    CMD_TOKEN_RIGHT_PAREN,
    CMD_TOKEN_LEFT_BRACE,
    CMD_TOKEN_RIGHT_BRACE,
    CMD_TOKEN_COMMA,
    CMD_TOKEN_SEMICOLON,
    CMD_TOKEN_DOT,
    CMD_TOKEN_COLON,
    CMD_TOKEN_ASSIGN,

    CMD_TOKEN_PLUS,
    CMD_TOKEN_MINUS,
    CMD_TOKEN_STAR,
    CMD_TOKEN_SLASH,
    CMD_TOKEN_PERCENT,
    CMD_TOKEN_BANG,
    CMD_TOKEN_EQUAL_EQUAL,
    CMD_TOKEN_BANG_EQUAL,
    CMD_TOKEN_LESS,
    CMD_TOKEN_LESS_EQUAL,
    CMD_TOKEN_GREATER,
    CMD_TOKEN_GREATER_EQUAL,
    CMD_TOKEN_AND_AND,
    CMD_TOKEN_OR_OR,

    /// lexed so the parser can say they are not supported yet
    CMD_TOKEN_PIPE,
    CMD_TOKEN_PIPE_GREATER,
    CMD_TOKEN_ARROW,
  };

  struct command_token {
    command_token_type type = CMD_TOKEN_INVALID;
    /// a name as written, a string with its escapes resolved, otherwise the source text
    std::string text;
    /// CMD_TOKEN_INTEGER only
    uint64_t value = 0;
    source_span span = {};
    /// 'f (x)' is a command with one word, 'f(x)' is a call
    bool space_before = false;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_TOKEN_HPP
