/**
 * \file vm/command_language/command_lowering.cpp
 **/
#include "vm/command_language/command_lowering.hpp"

#include <algorithm>
#include <ranges>

#include "core/profiler.hpp"

#include "vm/command_bus.hpp"
#include "vm/command_language/command_assets.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/diagnostics/ocmd_errors.hpp"
#include "vm/hooks.hpp"

namespace other {
  namespace {

    /// r0 to r7 carry arguments and rf the result, as for a syscall. r8 to rd hold the pieces of
    ///  the expression being worked out, and re is free inside any one step
    constexpr uint8_t kFirstTemp = 8;
    constexpr uint8_t kTempCount = 6;
    constexpr size_t kMaxArguments = 8;
    /// a text value is its length above this bit and its address below it
    constexpr uint64_t kTextLengthShift = 32;

    std::string reg(const size_t index) {
      return std::format("r{:x}", index);
    }

    bool is_literal(const command_expr& expr) {
      return expr.kind == CMD_EXPR_INTEGER || expr.kind == CMD_EXPR_STRING || expr.kind == CMD_EXPR_BOOL || expr.kind == CMD_EXPR_NULL;
    }

    bool contains_call(const command_expr& expr) {
      return expr.kind == CMD_EXPR_CALL || std::ranges::any_of(expr.operands, contains_call);
    }

    /// OASM reads a name made only of hex digits as a number and has keywords of its own, so no
    ///  script name reaches it bare: every generated name starts with a letter and '_'
    std::string sanitize(const std::string_view name) {
      std::string result{ name };
      for (char& c : result) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_' && c != '-') {
          c = '_';
        }
      }
      return result;
    }

    /// an OASM string cannot hold its own quote, a line break, or an escape; those go out as bytes
    std::string data_field_for_text(const std::string_view name, const std::string& text) {
      const bool plain = !text.empty() && std::ranges::all_of(text, [](const char c) { return c >= ' ' && c <= '~'; });
      if (plain && !text.contains('"')) {
        return std::format("  .{} : string = \"{}\"", name, text);
      }
      if (plain && !text.contains('\'')) {
        return std::format("  .{} : string = '{}'", name, text);
      }

      std::string bytes;
      for (const char c : text) {
        bytes += std::format("{:02x}", static_cast<uint8_t>(c));
      }
      /// an empty text still needs an address
      return std::format("  .{} : blob = 0x{}", name, bytes.empty() ? "00" : bytes);
    }

    std::string describe_parameters(const function_descriptor& function) {
      std::string result;
      size_t count = 0;
      for (size_t p = 0; p < function.params.size(); ++p) {
        if (function.params[p].kind == VM_TYPE_VOID) {
          continue;
        }
        std::string name{ function.params[p].name };
        if (function.params[p].kind == VM_TYPE_PTR && p + 1 < function.params.size() && function.params[p + 1].kind == VM_TYPE_U64) {
          name = std::format("{} (text)", name.ends_with("-ptr") ? name.substr(0, name.size() - 4) : name);
          ++p;
        }
        result += std::format("{}{}", count++ == 0 ? "" : ", ", name);
      }
      return std::format("{} argument(s){}{}", count, count == 0 ? "" : ": ", result);
    }

    std::string list_functions(const command_device& device) {
      std::string result;
      for (const function_descriptor& function : device.functions()) {
        result += std::format("{}{}", result.empty() ? "" : ", ", function.name);
      }
      return result;
    }

  }  // namespace

  command_lowering::command_lowering(const command_bus& bus, const std::string_view unit_name, std::span<const command_function_signature> imported, command_lowering_options options)
      : bus(bus), unit(unit_name), stem(options.stem.empty() ? unit_stem(unit_name) : options.stem), data_block(std::format("d_{}", stem)), options(options) {
    for (const command_function_signature& signature : imported) {
      functions[signature.name] = signature.param_count;
    }
  }

  std::string command_lowering::unit_stem(const std::string_view unit_name) {
    return sanitize(filepath{ unit_name }.stem().string());
  }

  std::string command_lowering::unit_path_stem(std::string_view unit_name) {
    return sanitize(filepath{ unit_name }.replace_extension().generic_string());
  }

  std::string command_lowering::text_field(std::string_view name, const std::string& text) {
    return data_field_for_text(name, text);
  }

  command_lowering_result command_lowering::lower(const command_file& file, diagnostic_engine* diag) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine must not be null");
    OTHER_ASSERT(file.valid, "Only a file that parsed can be lowered: {}", unit);
    PROFILE_SECTION("command_lowering::lower");
    diagnostics = diag;

    /// names first, so a function can use a function or a file-level variable written below it
    scopes.emplace_back();
    for (const command_function& function : file.functions) {
      guarded([&] { declare_function(function); });
    }
    for (const command_stmt& stmt : file.statements) {
      if (stmt.kind == CMD_STMT_LET) {
        guarded([&] { declare_file_variable(stmt); });
      }
    }

    command_lowering_result result;
    for (const command_function& function : file.functions) {
      guarded([&] { lower_function(function); });
    }
    for (const command_hook& hook : file.hooks) {
      guarded([&] { lower_hook(hook, result.hooks); });
    }
    lower_file_statements(file);
    check_recursion(file);

    result.success = !failed;
    if (result.success) {
      result.oasm = assemble(result.origins);
    }
    return result;
  }

  void command_lowering::fail(vm_error_code code, const source_span& span, const std::string& message) const {
    throw ocmd_toolchain_error(code, span, message);
  }

  void command_lowering::report(diagnostic_severity severity, vm_error_code code, const source_span& span, const std::string& message) {
    failed = failed || severity >= VM_DIAGNOSTIC_ERROR;
    diagnostics->emit({ .severity = severity, .error_code = code, .phase = VM_PHASE_SEMA, .span = span, .final_message = message });
  }

  /// one function, hook or file-level statement: an error ends it and the next one is still checked
  template <typename Fn>
  void command_lowering::guarded(Fn&& item) {
    reset_item();
    try {
      item();
    } catch (const ocmd_toolchain_error& e) {
      report(VM_DIAGNOSTIC_ERROR, e.error, e.loc, e.msg);
    }
  }

  void command_lowering::reset_item() {
    scopes.resize(1);
    loops.clear();
    live_temps = 0;
    current_origin = {};
    in_function = false;
    current_function.clear();
  }

  std::string command_lowering::new_field(const std::string_view prefix, const std::string_view name) {
    std::string field = name.empty() ? std::format("{}_{}", prefix, next_field++) : std::format("{}_{}_{}", prefix, next_field++, name);
    data.push_back(std::format("  .{} : = 0", field));
    return field;
  }

  uint8_t command_lowering::take_temp(const source_span& span) {
    if (live_temps == kTempCount) {
      fail(SEMA_NOT_SUPPORTED, span, "this expression is nested too deeply; move part of it into a 'let'");
    }
    return static_cast<uint8_t>(kFirstTemp + live_temps++);
  }

  size_t command_lowering::intern(const std::string& text) {
    const auto found = std::ranges::find(texts, text, &pooled_text::text);
    if (found != texts.end()) {
      return static_cast<size_t>(found - texts.begin());
    }
    texts.push_back({ .text = text });
    return texts.size() - 1;
  }

  opt<size_t> command_lowering::constant_text(const command_expr& expr) {
    if (expr.kind == CMD_EXPR_STRING) {
      return intern(expr.text);
    }
    if (expr.kind == CMD_EXPR_NAME) {
      const variable* found = find_variable(expr.text);
      return found != nullptr ? found->constant_text : std::nullopt;
    }
    return std::nullopt;
  }

  const command_lowering::variable* command_lowering::find_variable(const std::string& name) const {
    for (const auto& scope : scopes | std::views::reverse) {
      if (const auto found = scope.find(name); found != scope.end()) {
        return &found->second;
      }
    }
    return nullptr;
  }

  /// 'set' takes 8 bits and drops the rest without a word, and 'add' takes 16
  void command_lowering::load_integer(const std::string_view target, uint64_t value, const source_span& span) {
    if (value <= 0xFF) {
      emit("set {}, {}", target, value);
    } else if (value <= 0xFFFF) {
      emit("set {}, 0", target);
      emit("add {}, {}", target, value);
    } else if (value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
      const std::string field = std::format("k_{}", next_field++);
      data.push_back(std::format("  .{} : = {}", field, value));
      emit("set {}, [{}.{}]", target, data_block, field);
    } else {
      fail(SEMA_NOT_SUPPORTED, span, std::format("{} is too large; numbers above {} are not supported yet", value, std::numeric_limits<int64_t>::max()));
    }
  }

  void command_lowering::load_text(const std::string_view target, size_t text) {
    texts[text].packed = true;
    emit("set {}, {}.s_{}", target, data_block, text);
    emit("set re, [{}.s_{}_hi]", data_block, text);
    emit("or {0}, re, {0}", target);
  }

  void command_lowering::declare_function(const command_function& function) {
    if (functions.contains(function.name)) {
      fail(SEMA_DUPLICATE_NAME, function.span, std::format("there is already a function named '{}' in this build", function.name));
    }
    if (function.params.size() > kMaxArguments) {
      fail(SEMA_NOT_SUPPORTED, function.span, std::format("'{}' has {} parameters; a function can have {}", function.name, function.params.size(), kMaxArguments));
    }
    functions[function.name] = function.params.size();
  }

  /// a file-level 'let' with a literal on the right needs no code, so it holds its value in every
  ///  build, including one where this file's statements are not run
  void command_lowering::declare_file_variable(const command_stmt& stmt) {
    if (scopes.front().contains(stmt.name)) {
      fail(SEMA_DUPLICATE_NAME, stmt.span, std::format("'{}' is already declared in this file", stmt.name));
    }

    /// a variable given an asset stands for the asset's name, which is what the build knows it by
    if (!stmt.declared.empty()) {
      scopes.front()[stmt.name] = { .constant_text = intern(command_asset_lowering::bound_name(stmt)) };
      return;
    }
    if (const auto asset = options.included.find(stmt.included); !stmt.included.empty() && asset != options.included.end()) {
      scopes.front()[stmt.name] = { .constant_text = intern(asset->second) };
      return;
    }

    const command_expr& value = stmt.exprs.front();
    if (value.kind == CMD_EXPR_STRING) {
      scopes.front()[stmt.name] = { .constant_text = intern(value.text) };
      return;
    }

    const std::string field = std::format("v_{}", stmt.name);
    uint64_t start = 0;
    if (value.kind == CMD_EXPR_INTEGER || value.kind == CMD_EXPR_BOOL) {
      if (value.integer > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        fail(SEMA_NOT_SUPPORTED, value.span, std::format("{} is too large; numbers above {} are not supported yet", value.integer, std::numeric_limits<int64_t>::max()));
      }
      start = value.integer;
    }
    data.push_back(std::format("  .{} : = {}", field, start));
    scopes.front()[stmt.name] = { .field = field };
  }

  void command_lowering::lower_function(const command_function& function) {
    in_function = true;
    current_function = function.name;
    calls[function.name];

    add_line(std::format("$f_{}:", function.name));
    /// the lines that take the arguments and the closing return belong to the line with 'fn' on it
    const origin_scope origin(*this, function.span.start);
    scopes.emplace_back();
    for (size_t i = 0; i < function.params.size(); ++i) {
      if (scopes.back().contains(function.params[i])) {
        fail(SEMA_DUPLICATE_NAME, function.span, std::format("'{}' has two parameters named '{}'", function.name, function.params[i]));
      }
      const std::string field = new_field("l", function.params[i]);
      emit("write {}, {}.{}", reg(i), data_block, field);
      scopes.back()[function.params[i]] = { .field = field };
    }
    for (const command_stmt& stmt : function.body) {
      lower_statement(stmt);
    }

    /// reached when the body ends without a 'return': the result is null and the status is success
    emit("set rf, 0");
    emit("set rflag, 0");
    emit("ret");
    add_line("end");
    add_line("");
  }

  void command_lowering::lower_hook(const command_hook& hook, ostd::vector<command_hook_record>& records) {
    if (!hook.engine_event && !get_hook_id(hook.name).has_value()) {
      std::string names;
      for (const auto& h : vm_hooks()) {
        names += std::format("{}{}", names.empty() ? "" : ", ", h.name);
      }
      fail(SEMA_UNKNOWN_NAME, hook.span, std::format("'{}' is not a VM hook. the hooks are: {}; an engine event is written in quotes", hook.name, names));
    }

    const std::string block = std::format("h_{}_{}", stem, sanitize(hook.name));
    if (std::ranges::find(records, block, &command_hook_record::block) != records.end()) {
      fail(SEMA_DUPLICATE_NAME, hook.span, std::format("this file already has a handler for '{}'", hook.name));
    }
    in_function = true;

    add_line(std::format("${}:", block));
    const origin_scope origin(*this, hook.span.start);
    lower_block(hook.body);
    emit("set rf, 0");
    emit("set rflag, 0");
    emit("ret");
    add_line("end");
    add_line("");
    records.push_back({ .name = hook.name, .engine_event = hook.engine_event, .block = block, .unit = unit });
  }

  void command_lowering::lower_file_statements(const command_file& file) {
    const auto runs_code = [](const command_stmt& stmt) {
      return stmt.kind != CMD_STMT_LET || (stmt.declared.empty() && !is_literal(stmt.exprs.front()));
    };
    if (!options.entry_unit) {
      const auto first = std::ranges::find_if(file.statements, runs_code);
      if (first != file.statements.end()) {
        report(options.strict ? VM_DIAGNOSTIC_ERROR : VM_DIAGNOSTIC_WARNING, SEMA_DROPPED_STATEMENTS, first->span,
               std::format("'{}' is not the entry file, so the statements outside its functions are not run", unit));
      }
      return;
    }

    add_line(std::format("$m_{}:", stem));
    for (const command_stmt& stmt : file.statements) {
      guarded([&] { lower_statement(stmt); });
    }
    emit("stopdev");
    add_line("end");
  }

  /// a function's variables are fields, one copy each, so a second live call would overwrite the first
  void command_lowering::check_recursion(const command_file& file) {
    for (const command_function& function : file.functions) {
      ostd::vector<std::string> pending = calls[function.name];
      ostd::vector<std::string> seen;
      while (!pending.empty()) {
        const std::string callee = pending.back();
        pending.pop_back();
        if (callee == function.name) {
          report(VM_DIAGNOSTIC_ERROR, SEMA_NOT_SUPPORTED, function.span,
                 std::format("'{}' calls itself, directly or through another function; that is not supported yet", function.name));
          break;
        }
        if (std::ranges::find(seen, callee) == seen.end()) {
          seen.push_back(callee);
          if (const auto next = calls.find(callee); next != calls.end()) {
            pending.append_range(next->second);
          }
        }
      }
    }
  }

  std::string command_lowering::assemble(ostd::vector<source_location>& origins) const {
    std::string text = std::format("; generated from {}\n", unit);
    if (options.entry_unit) {
      text += std::format("/entry: m_{}\n", stem);
    }
    text += "\n";

    if (!data.empty() || !texts.empty()) {
      text += std::format("#{} {{\n", data_block);
      for (const std::string& line : data) {
        text += std::format("{}\n", line);
      }
      for (size_t i = 0; i < texts.size(); ++i) {
        text += std::format("{}\n", text_field(std::format("s_{}", i), texts[i].text));
        if (texts[i].packed) {
          text += std::format("  .s_{}_hi : = {}\n", i, uint64_t{ texts[i].text.size() } << kTextLengthShift);
        }
      }
      text += "}\n\n";
    }

    /// everything written so far is the heading and the data, which stand for no statement
    origins.assign(static_cast<size_t>(std::ranges::count(text, '\n')), source_location{});
    for (size_t i = 0; i < code.size(); ++i) {
      text += std::format("{}\n", code[i]);
      origins.push_back(code_origins[i]);
    }
    return text;
  }

  void command_lowering::lower_block(std::span<const command_stmt> statements) {
    scopes.emplace_back();
    for (const command_stmt& stmt : statements) {
      lower_statement(stmt);
    }
    scopes.pop_back();
  }

  void command_lowering::lower_statement(const command_stmt& stmt) {
    const origin_scope origin(*this, stmt.span.start);
    switch (stmt.kind) {
      case CMD_STMT_LET: lower_let(stmt); break;

      case CMD_STMT_ASSIGN: {
        const variable* target = find_variable(stmt.name);
        if (target == nullptr) {
          fail(SEMA_UNKNOWN_NAME, stmt.span, std::format("'{}' is not a variable; declare it with 'let {} = ...'", stmt.name, stmt.name));
        }
        if (target->constant_text.has_value()) {
          fail(SEMA_NOT_SUPPORTED, stmt.span, std::format("'{}' is a file-level text and cannot be given a new value yet", stmt.name));
        }
        const std::string field = target->field;
        const lowered value = lower_expression(stmt.exprs.front());
        emit("write {}, {}.{}", reg(value.reg), data_block, field);
        release_temp();
      } break;

      case CMD_STMT_COMMANDS:
        for (size_t i = 0; i < stmt.exprs.size(); ++i) {
          /// a command leaves its status in rflag, 0 for success. a skipped command leaves it alone
          std::string skip;
          if (i > 0) {
            skip = new_label();
            emit("{} {}", stmt.joins[i - 1] == CMD_TOKEN_AND_AND ? "jne" : "je", skip);
          }
          lower_call(stmt.exprs[i], false);
          if (!skip.empty()) {
            place_label(skip);
          }
        }
        break;

      case CMD_STMT_IF: {
        const std::string otherwise = new_label();
        lower_condition(stmt.exprs.front(), otherwise);
        lower_block(stmt.body);
        if (stmt.else_body.empty()) {
          place_label(otherwise);
          break;
        }
        const std::string end = new_label();
        emit("goto {}", end);
        place_label(otherwise);
        lower_block(stmt.else_body);
        place_label(end);
      } break;

      case CMD_STMT_WHILE: {
        const loop_labels labels = { .top = new_label(), .end = new_label() };
        place_label(labels.top);
        lower_condition(stmt.exprs.front(), labels.end);
        loops.push_back(labels);
        lower_block(stmt.body);
        loops.pop_back();
        emit("goto {}", labels.top);
        place_label(labels.end);
      } break;

      case CMD_STMT_RETURN:
        if (!in_function) {
          fail(SEMA_MISPLACED_STATEMENT, stmt.span, "'return' is only allowed inside a function or a handler");
        }
        if (stmt.exprs.empty()) {
          emit("set rf, 0");
        } else {
          const lowered value = lower_expression(stmt.exprs.front());
          emit("mov {}, rf", reg(value.reg));
          release_temp();
        }
        emit("set rflag, 0");
        emit("ret");
        break;

      case CMD_STMT_BREAK:
      case CMD_STMT_CONTINUE:
        if (loops.empty()) {
          fail(SEMA_MISPLACED_STATEMENT, stmt.span, std::format("'{}' is only allowed inside a 'while'", stmt.kind == CMD_STMT_BREAK ? "break" : "continue"));
        }
        emit("goto {}", stmt.kind == CMD_STMT_BREAK ? loops.back().end : loops.back().top);
        break;

      default: OTHER_ASSERT(false, "Unhandled statement kind {}", static_cast<uint32_t>(stmt.kind));
    }
  }

  void command_lowering::lower_let(const command_stmt& stmt) {
    /// an asset is not made by running anything; declare_file_variable gave the variable its name
    if (!stmt.declared.empty()) {
      return;
    }
    const command_expr& value = stmt.exprs.front();
    const bool file_level = !in_function && scopes.size() == 1;
    if (file_level) {
      /// declare_file_variable made the field, and gave it the value when that is a literal
      const variable* target = find_variable(stmt.name);
      if (target == nullptr || is_literal(value)) {
        return;
      }
      const std::string field = target->field;
      const lowered result = lower_expression(value);
      emit("write {}, {}.{}", reg(result.reg), data_block, field);
      release_temp();
      return;
    }

    if (scopes.back().contains(stmt.name)) {
      fail(SEMA_DUPLICATE_NAME, stmt.span, std::format("'{}' is already declared in this block", stmt.name));
    }
    /// the value is worked out first, so 'let x = x + 1' reads the outer x
    const lowered result = lower_expression(value);
    const std::string field = new_field("l", stmt.name);
    emit("write {}, {}.{}", reg(result.reg), data_block, field);
    release_temp();
    scopes.back()[stmt.name] = { .field = field };
  }

  /// je and jne read rflag and nothing else, so the value is moved there to branch on it
  void command_lowering::lower_condition(const command_expr& condition, const std::string& false_label) {
    const lowered value = lower_expression(condition);
    emit("mov {}, rflag", reg(value.reg));
    release_temp();
    emit("je {}", false_label);
  }

  command_lowering::lowered command_lowering::lower_expression(const command_expr& expr) {
    switch (expr.kind) {
      case CMD_EXPR_INTEGER:
      case CMD_EXPR_BOOL:
      case CMD_EXPR_NULL: {
        const uint8_t target = take_temp(expr.span);
        load_integer(reg(target), expr.kind == CMD_EXPR_NULL ? 0 : expr.integer, expr.span);
        return { .reg = target, .boolean = expr.kind == CMD_EXPR_BOOL };
      }

      case CMD_EXPR_STRING: {
        const uint8_t target = take_temp(expr.span);
        load_text(reg(target), intern(expr.text));
        return { .reg = target };
      }

      case CMD_EXPR_NAME: {
        const variable* source = find_variable(expr.text);
        if (source == nullptr) {
          fail(SEMA_UNKNOWN_NAME, expr.span, std::format("'{}' is not a variable. a command used as a value is written '{}()'", expr.text, expr.text));
        }
        const uint8_t target = take_temp(expr.span);
        if (source->constant_text.has_value()) {
          load_text(reg(target), *source->constant_text);
        } else {
          emit("set {}, [{}.{}]", reg(target), data_block, source->field);
        }
        return { .reg = target };
      }

      case CMD_EXPR_UNARY: {
        lowered value = lower_expression(expr.operands.front());
        if (expr.op == CMD_TOKEN_BANG) {
          invert(value.reg);
          value.boolean = true;
        } else {
          emit("set re, 0");
          emit("sub re, {}", reg(value.reg));
          emit("mov re, {}", reg(value.reg));
          value.boolean = false;
        }
        return value;
      }

      case CMD_EXPR_BINARY: return lower_binary(expr);

      case CMD_EXPR_CALL: return { .reg = *lower_call(expr, true) };

      default: OTHER_ASSERT(false, "Unhandled expression kind {}", static_cast<uint32_t>(expr.kind));
    }
    return {};
  }

  command_lowering::lowered command_lowering::lower_binary(const command_expr& expr) {
    lowered left = lower_expression(expr.operands[0]);
    const std::string a = reg(left.reg);

    if (expr.op == CMD_TOKEN_AND_AND || expr.op == CMD_TOKEN_OR_OR) {
      /// the right side is skipped when the left side already decides the answer
      to_boolean(left);
      const std::string decided = new_label();
      emit("mov {}, rflag", a);
      emit("{} {}", expr.op == CMD_TOKEN_AND_AND ? "je" : "jne", decided);
      lowered right = lower_expression(expr.operands[1]);
      to_boolean(right);
      emit("mov {}, {}", reg(right.reg), a);
      release_temp();
      place_label(decided);
      return left;
    }

    const lowered right = lower_expression(expr.operands[1]);
    const std::string b = reg(right.reg);
    left.boolean = true;
    switch (expr.op) {
      case CMD_TOKEN_PLUS:
        emit("add {}, {}", a, b);
        left.boolean = false;
        break;
      case CMD_TOKEN_MINUS:
        emit("sub {}, {}", a, b);
        left.boolean = false;
        break;
      case CMD_TOKEN_STAR:
        emit("mul {}, {}", a, b);
        left.boolean = false;
        break;
      case CMD_TOKEN_SLASH:
        emit("div {}, {}", a, b);
        left.boolean = false;
        break;
      case CMD_TOKEN_PERCENT:
        emit("mod {}, {}", a, b);
        left.boolean = false;
        break;
      case CMD_TOKEN_EQUAL_EQUAL: emit("cmp {0}, {1}, {0}", a, b); break;
      case CMD_TOKEN_LESS: emit("cmplt {0}, {1}, {0}", a, b); break;
      case CMD_TOKEN_GREATER: emit("cmpgt {0}, {1}, {0}", a, b); break;
      case CMD_TOKEN_BANG_EQUAL:
        emit("cmp {0}, {1}, {0}", a, b);
        invert(left.reg);
        break;
      case CMD_TOKEN_LESS_EQUAL:
        emit("cmpgt {0}, {1}, {0}", a, b);
        invert(left.reg);
        break;
      case CMD_TOKEN_GREATER_EQUAL:
        emit("cmplt {0}, {1}, {0}", a, b);
        invert(left.reg);
        break;
      default: OTHER_ASSERT(false, "Unhandled binary operator {}", static_cast<uint32_t>(expr.op));
    }
    release_temp();
    return left;
  }

  void command_lowering::to_boolean(lowered& value) {
    if (!value.boolean) {
      invert(value.reg);
      invert(value.reg);
      value.boolean = true;
    }
  }

  /// 0 becomes 1 and anything else becomes 0
  void command_lowering::invert(uint8_t target) {
    emit("set re, 0");
    emit("cmp {0}, re, {0}", reg(target));
  }

  /// lookup order: this file's functions, the other files' functions, then the drivers
  command_lowering::call_target command_lowering::resolve(const command_expr& call) const {
    if (call.member.empty()) {
      if (const auto function = functions.find(call.text); function != functions.end()) {
        return { .symbol = std::format("f_{}", call.text), .param_count = function->second };
      }
    }

    const command_device* device = bus.find_device_by_name(call.text);
    if (device == nullptr) {
      fail(SEMA_UNKNOWN_NAME, call.span,
           call.member.empty() ? std::format("'{}' is not a function in this build and not a driver", call.text) : std::format("'{}' is not a driver", call.text));
    }

    /// in 'driver word word' the first word is the verb when the driver has a command of that name
    std::string verb = call.member;
    size_t first_argument = 0;
    if (verb.empty()) {
      if (!call.word_form || call.operands.empty() || call.operands.front().kind != CMD_EXPR_NAME) {
        fail(SEMA_BAD_CALL, call.span, std::format("'{}' is a driver; name one of its commands: {}", call.text, list_functions(*device)));
      }
      verb = call.operands.front().text;
      first_argument = 1;
    }
    std::ranges::replace(verb, '-', '_');

    for (const function_descriptor& function : device->functions()) {
      if (function.name == verb) {
        return { .syscall = &function, .symbol = std::format("{}.{}", device->device_name(), function.name), .first_argument = first_argument };
      }
    }
    fail(SEMA_UNKNOWN_NAME, call.span, std::format("driver '{}' has no command '{}'; it has: {}", call.text, verb, list_functions(*device)));
  }

  /// one script argument per parameter, except that a text fills a pointer and the length after it
  ostd::vector<command_lowering::machine_argument> command_lowering::match_arguments(const command_expr& call, const call_target& target) const {
    const std::span<const command_expr> sources = std::span<const command_expr>{ call.operands }.subspan(target.first_argument);
    ostd::vector<machine_argument> arguments;

    if (target.syscall == nullptr) {
      if (sources.size() != target.param_count) {
        fail(SEMA_BAD_CALL, call.span, std::format("'{}' takes {} argument(s) and was given {}", call.text, target.param_count, sources.size()));
      }
      for (const command_expr& source : sources) {
        arguments.push_back({ .source = &source });
      }
      return arguments;
    }

    const std::span<const type_descriptor> params = target.syscall->params;
    size_t used = 0;
    bool fits = true;
    for (size_t p = 0; p < params.size() && fits; ++p) {
      if (params[p].kind == VM_TYPE_VOID) {
        continue;
      }
      if (used == sources.size()) {
        fits = false;
      } else if (params[p].kind == VM_TYPE_PTR && p + 1 < params.size() && params[p + 1].kind == VM_TYPE_U64) {
        arguments.push_back({ .source = &sources[used], .part = ARGUMENT_TEXT_POINTER });
        arguments.push_back({ .source = &sources[used], .part = ARGUMENT_TEXT_LENGTH });
        ++used;
        ++p;
      } else {
        arguments.push_back({ .source = &sources[used++] });
      }
    }
    if (!fits || used != sources.size() || arguments.size() > kMaxArguments) {
      fail(SEMA_BAD_CALL, call.span, std::format("'{}' takes {} and was given {}", target.symbol, describe_parameters(*target.syscall), sources.size()));
    }
    return arguments;
  }

  void command_lowering::load_argument(const std::string_view target, const machine_argument& argument) {
    const command_expr& source = *argument.source;
    if (!argument.spill.empty()) {
      emit("set {}, [{}.{}]", target, data_block, argument.spill);
    } else if (const opt<size_t> text = constant_text(source); text.has_value()) {
      load_text(target, *text);
    } else if (is_literal(source)) {
      load_integer(target, source.kind == CMD_EXPR_NULL ? 0 : source.integer, source.span);
    } else if (const variable* held = source.kind == CMD_EXPR_NAME ? find_variable(source.text) : nullptr; held != nullptr) {
      emit("set {}, [{}.{}]", target, data_block, held->field);
    } else {
      const lowered value = lower_expression(source);
      emit("mov {}, {}", reg(value.reg), target);
      release_temp();
    }
  }

  opt<uint8_t> command_lowering::lower_call(const command_expr& call, bool want_result) {
    /// a call gets its own column, so two commands on one line can be told apart
    const origin_scope origin(*this, call.span.start);
    const call_target target = resolve(call);
    ostd::vector<machine_argument> arguments = match_arguments(call, target);

    /// a call inside one argument would overwrite the argument registers already filled, so
    ///  when any argument makes a call each one is worked out in order and kept in a field
    if (std::ranges::any_of(arguments, [](const machine_argument& argument) { return contains_call(*argument.source); })) {
      for (machine_argument& argument : arguments) {
        if (argument.part == ARGUMENT_TEXT_LENGTH || is_literal(*argument.source) || constant_text(*argument.source).has_value()) {
          continue;
        }
        const lowered value = lower_expression(*argument.source);
        argument.spill = new_field("t");
        emit("write {}, {}.{}", reg(value.reg), data_block, argument.spill);
        release_temp();
      }
    }

    /// a script function uses r8 and up for its own expressions; a syscall leaves them alone
    ostd::vector<std::string> saved;
    if (target.syscall == nullptr) {
      for (uint8_t temp = 0; temp < live_temps; ++temp) {
        saved.push_back(new_field("t"));
        emit("write {}, {}.{}", reg(kFirstTemp + temp), data_block, saved.back());
      }
    }

    for (size_t i = 0; i < arguments.size(); ++i) {
      const machine_argument& argument = arguments[i];
      if (argument.part == ARGUMENT_TEXT_LENGTH) {
        continue;
      }
      if (argument.part == ARGUMENT_VALUE) {
        load_argument(reg(i), argument);
        continue;
      }

      const std::string pointer = reg(i);
      const std::string length = reg(i + 1);
      if (const opt<size_t> text = argument.spill.empty() ? constant_text(*argument.source) : std::nullopt; text.has_value()) {
        emit("set {}, {}.s_{}", pointer, data_block, *text);
        load_integer(length, texts[*text].text.size(), argument.source->span);
        continue;
      }
      /// a text held in a variable: split the one value back into its address and its length
      load_argument(pointer, argument);
      emit("mov {}, {}", pointer, length);
      emit("set re, {}", kTextLengthShift);
      emit("rshift {}, re", length);
      emit("lshift {}, re", pointer);
      emit("rshift {}, re", pointer);
    }

    if (target.syscall != nullptr) {
      /// the devices read rflag as well as write it: a value left there makes a text argument fail
      emit("set rflag, 0");
      emit("syscall {}", target.symbol);
    } else {
      emit("call {}", target.symbol);
      if (!current_function.empty()) {
        calls[current_function].push_back(call.text);
      }
    }

    for (size_t temp = 0; temp < saved.size(); ++temp) {
      emit("set {}, [{}.{}]", reg(kFirstTemp + temp), data_block, saved[temp]);
    }

    if (!want_result) {
      return std::nullopt;
    }
    if (target.syscall != nullptr && target.syscall->returns.kind == VM_TYPE_VOID) {
      fail(SEMA_BAD_CALL, call.span, std::format("'{}' does not give back a value", target.symbol));
    }
    const uint8_t result = take_temp(call.span);
    emit("mov rf, {}", reg(result));
    return result;
  }

}  // namespace other
