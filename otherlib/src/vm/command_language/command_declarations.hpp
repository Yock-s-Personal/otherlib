/**
 * \file vm/command_language/command_declarations.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_DECLARATIONS_HPP
#define OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_DECLARATIONS_HPP

#include <span>

#include "vm/command_language/command_ast.hpp"
#include "vm/diagnostics/error_codes.hpp"

namespace other {

  class diagnostic_engine;

  struct command_check_counts {
    size_t enums = 0;
    size_t structs = 0;
    /// every declaration, the ones inside others among them
    size_t declarations = 0;
  };

  /// holds every declaration of a build to the struct it is a declaration of. nothing is
  ///  made here: a build that passes is one whose declarations mean something.
  ///  every file is added before any is checked, since a struct, an enum or a declaration
  ///  of one file can be named in another; the files have to outlive the checker
  class command_declaration_checker {
   public:
    void add(const command_file& file);

    /// the caller says which file an error is in: a diagnostic carries a line and a column
    ///  and no file. false when this file had an error.
    ///  bound: the file was given to a variable, which is what its declaration is known by
    bool check(const command_file& file, diagnostic_engine* diag, bool bound = false);

    const command_check_counts& counts() const { return counted; }

    /// a declaration and the one it is written inside
    struct scope {
      const command_declaration* declaration = nullptr;
      const scope* outer = nullptr;
    };

    const command_enum* find_enum(std::string_view name) const;
    const command_struct* find_struct(std::string_view name) const;
    /// a declaration of one of kinds named name, looked for from the innermost scope out,
    ///  and then, unless outside is false, among what the build declares outside everything
    const command_declaration* find_declared(std::span<const std::string> kinds, std::string_view name, const scope* from, bool outside = true) const;

   private:
    void error(vm_error_code code, const source_span& span, std::string message);

    void check_enum(const command_enum& checked);
    void check_struct(const command_struct& checked);
    void check_declaration(const command_declaration& declaration, const scope* outer, bool needs_name);
    void check_edges(const command_declaration& declaration, const command_struct& kind);
    /// whether value is what a field of this type takes. says why not when it is not
    bool check_value(const command_struct& owner, const command_struct_field& field, bool as_item, const command_value& value, const scope* from);

    ostd::vector<const command_enum*> enums;
    ostd::vector<const command_struct*> structs;
    ostd::vector<const command_declaration*> top_level;

    diagnostic_engine* diagnostics = nullptr;
    bool failed = false;
    command_check_counts counted;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_DECLARATIONS_HPP
