/**
 * \file vm/command_language/command_lowering.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_LOWERING_HPP
#define OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_LOWERING_HPP

#include <span>

#include "data-structures/std_container.hpp"

#include "vm/command_language/command_ast.hpp"
#include "vm/vm_function.hpp"

namespace other {

  class command_bus;
  class diagnostic_engine;

  /// a function another file of the same build defines
  struct command_function_signature {
    std::string name;
    size_t param_count = 0;
  };

  struct command_hook_record {
    /// pre_load and the like, or the engine event's name
    std::string name;
    bool engine_event = false;
    /// the OASM block that holds the handler
    std::string block;
  };

  struct command_lowering_options {
    /// the statements outside functions become the program's entry. in any other file they
    ///  are not run, which is a warning, or an error when strict is set
    bool entry_unit = true;
    bool strict = false;
  };

  struct command_lowering_result {
    /// one OASM unit; empty unless success
    std::string oasm;
    ostd::vector<command_hook_record> hooks;
    bool success = false;
  };

  /// turns one parsed .ocmd file into OASM text. every variable is an 8-byte data field, so the
  ///  registers only ever hold the pieces of one expression
  class command_lowering {
   public:
    /// commands resolve against the devices on bus, so it must hold what the loading driver registers
    command_lowering(const command_bus& bus, std::string_view unit_name, std::span<const command_function_signature> imported, command_lowering_options options = {});

    command_lowering_result lower(const command_file& file, diagnostic_engine* diag);

    /// the file name's stem, with anything OASM cannot hold in a name turned into '_'
    static std::string unit_stem(std::string_view unit_name);

   private:
    struct variable {
      std::string field;
      /// set for a file-level 'let name = "text"': the name stands for that text and has no field
      opt<size_t> constant_text = std::nullopt;
    };

    struct pooled_text {
      std::string text;
      /// some use needs the address and length as one value
      bool packed = false;
    };

    struct lowered {
      uint8_t reg = 0;
      /// known to hold 0 or 1
      bool boolean = false;
    };

    struct loop_labels {
      std::string top;
      std::string end;
    };

    struct call_target {
      const function_descriptor* syscall = nullptr;
      /// 'device.function', or the block of a script function
      std::string symbol;
      /// a command's first word when it was the verb
      size_t first_argument = 0;
      size_t param_count = 0;
    };

    enum argument_part : uint8_t {
      ARGUMENT_VALUE,
      ARGUMENT_TEXT_POINTER,
      ARGUMENT_TEXT_LENGTH,
    };

    struct machine_argument {
      const command_expr* source = nullptr;
      argument_part part = ARGUMENT_VALUE;
      /// the field its value waits in when another argument makes a call
      std::string spill;
    };

    [[noreturn]] void fail(vm_error_code code, const source_span& span, const std::string& message) const;
    void report(diagnostic_severity severity, vm_error_code code, const source_span& span, const std::string& message);
    template <typename Fn>
    void guarded(Fn&& item);
    void reset_item();

    template <typename... Args>
    void emit(std::format_string<Args...> format, Args&&... args) {
      code.push_back(std::format("  {}", std::format(format, std::forward<Args>(args)...)));
    }
    std::string new_label() { return std::format("l_{}", next_label++); }
    void place_label(const std::string& label) { code.push_back(std::format("@{}:", label)); }
    std::string new_field(std::string_view prefix, std::string_view name = "");
    uint8_t take_temp(const source_span& span);
    void release_temp() { --live_temps; }

    size_t intern(const std::string& text);
    opt<size_t> constant_text(const command_expr& expr);
    const variable* find_variable(const std::string& name) const;
    void load_integer(std::string_view target, uint64_t value, const source_span& span);
    void load_text(std::string_view target, size_t text);

    void declare_function(const command_function& function);
    void declare_file_variable(const command_stmt& stmt);
    void lower_function(const command_function& function);
    void lower_hook(const command_hook& hook, ostd::vector<command_hook_record>& records);
    void lower_file_statements(const command_file& file);
    void check_recursion(const command_file& file);
    std::string assemble() const;

    void lower_block(std::span<const command_stmt> statements);
    void lower_statement(const command_stmt& stmt);
    void lower_let(const command_stmt& stmt);
    void lower_condition(const command_expr& condition, const std::string& false_label);

    lowered lower_expression(const command_expr& expr);
    lowered lower_binary(const command_expr& expr);
    void to_boolean(lowered& value);
    void invert(uint8_t reg);

    call_target resolve(const command_expr& call) const;
    ostd::vector<machine_argument> match_arguments(const command_expr& call, const call_target& target) const;
    void load_argument(std::string_view target, const machine_argument& argument);
    opt<uint8_t> lower_call(const command_expr& call, bool want_result);

    const command_bus& bus;
    std::string unit;
    std::string stem;
    std::string data_block;
    command_lowering_options options;
    diagnostic_engine* diagnostics = nullptr;
    bool failed = false;

    ostd::vector<std::string> data;
    ostd::vector<std::string> code;
    ostd::vector<pooled_text> texts;
    ostd::vector<ostd::unordered_map<std::string, variable>> scopes;
    ostd::unordered_map<std::string, size_t> functions;
    /// script function -> the script functions it calls
    ostd::unordered_map<std::string, ostd::vector<std::string>> calls;
    ostd::vector<loop_labels> loops;

    std::string current_function;
    bool in_function = false;
    size_t next_label = 0;
    size_t next_field = 0;
    uint8_t live_temps = 0;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_LOWERING_HPP
