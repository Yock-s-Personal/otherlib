/**
 * \file vm/command_language/command_lexer.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_LEXER_HPP
#define OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_LEXER_HPP

#include "data-structures/std_container.hpp"

#include "vm/command_language/command_token.hpp"

namespace other {

  class diagnostic_engine;

  /// the lexer for .ocmd text. ocmd_lexer reads .oasm and cannot be shared: it drops newlines,
  ///  has no operators, and reads any word made of hex digits as a number
  class command_lexer {
   public:
    explicit command_lexer(const std::string_view source)
        : source(source) {}

    /// the result always ends in CMD_TOKEN_EOF; check failed() before parsing it
    ostd::vector<command_token> tokenize(diagnostic_engine* diag);
    bool failed() const { return had_error; }

   private:
    bool finished() const { return cursor >= source.size(); }
    char peek(size_t ahead = 0) const { return cursor + ahead < source.size() ? source[cursor + ahead] : '\0'; }
    char advance();
    source_location here() const { return { line, column, cursor }; }

    void add(command_token_type type, std::string text, uint64_t value = 0);
    void error(vm_error_code code, std::string message);

    void lex_newline();
    void lex_number();
    void lex_name();
    void lex_string();
    void lex_operator();

    std::string_view source;
    diagnostic_engine* diagnostics = nullptr;
    ostd::vector<command_token> tokens;

    size_t cursor = 0;
    size_t line = 1;
    size_t column = 1;
    source_location token_start = {};
    size_t paren_depth = 0;
    bool space_pending = false;
    bool had_error = false;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_LEXER_HPP
