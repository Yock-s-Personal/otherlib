/**
 * \file vm/command_language/command_declarations.cpp
 **/
#include "vm/command_language/command_declarations.hpp"

#include "core/profiler.hpp"

#include "vm/diagnostics/diagnostic_engine.hpp"

namespace other {
  namespace {

    constexpr std::string_view kPlainTypes[] = { "int", "float", "bool", "text", "name", "asset", "any" };
    constexpr std::string_view kPlainTypeList = "int, float, bool, text, name, asset, any";

    bool is_plain_type(const std::string_view name) {
      return std::ranges::find(kPlainTypes, name) != std::end(kPlainTypes);
    }

    std::string_view describe(const command_value& value) {
      switch (value.kind) {
        case CMD_VALUE_INTEGER: return "a whole number";
        case CMD_VALUE_DECIMAL: return "a number with a point";
        case CMD_VALUE_BOOL: return "true or false";
        case CMD_VALUE_TEXT: return "a text";
        case CMD_VALUE_NAME: return "a name";
        case CMD_VALUE_LIST: return "a list";
        case CMD_VALUE_NULL: return "null";
        default: return "a value";
      }
    }

    /// "pass 'geometry'", or "this pass" for one with no name
    std::string label(const command_declaration& declaration) {
      return declaration.name.empty() ? std::format("this {}", declaration.kind) : std::format("{} '{}'", declaration.kind, declaration.name);
    }

    std::string joined(const std::span<const std::string> words, const std::string_view between) {
      std::string text;
      for (size_t i = 0; i < words.size(); ++i) {
        text += i == 0 ? "" : between;
        text += words[i];
      }
      return text;
    }

    const command_struct_field* field_of(const command_struct& kind, const std::string_view name) {
      const auto found = std::ranges::find(kind.fields, name, &command_struct_field::name);
      return found != kind.fields.end() ? &*found : nullptr;
    }

  }  // namespace

  void command_declaration_checker::add(const command_file& file) {
    for (const command_enum& declared : file.enums) {
      enums.push_back(&declared);
    }
    for (const command_struct& declared : file.structs) {
      structs.push_back(&declared);
    }
    for (const command_declaration& declared : file.declarations) {
      top_level.push_back(&declared);
    }
    for (const command_stmt& stmt : file.statements) {
      for (const command_declaration& declared : stmt.declared) {
        top_level.push_back(&declared);
      }
    }
  }

  bool command_declaration_checker::check(const command_file& file, diagnostic_engine* diag, const bool bound) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine must not be null");
    PROFILE_SECTION("command_declaration_checker::check");
    diagnostics = diag;
    failed = false;

    for (const command_enum& declared : file.enums) {
      check_enum(declared);
    }
    for (const command_struct& declared : file.structs) {
      check_struct(declared);
    }
    for (const command_declaration& declared : file.declarations) {
      check_declaration(declared, nullptr, !bound);
    }
    for (const command_stmt& stmt : file.statements) {
      /// one bound to a variable is known by the variable
      for (const command_declaration& declared : stmt.declared) {
        check_declaration(declared, nullptr, false);
      }
    }
    return !failed;
  }

  const command_enum* command_declaration_checker::find_enum(const std::string_view name) const {
    const auto found = std::ranges::find(enums, name, &command_enum::name);
    return found != enums.end() ? *found : nullptr;
  }

  const command_struct* command_declaration_checker::find_struct(const std::string_view name) const {
    const auto found = std::ranges::find(structs, name, &command_struct::name);
    return found != structs.end() ? *found : nullptr;
  }

  const command_declaration* command_declaration_checker::find_declared(const std::span<const std::string> kinds, const std::string_view name, const scope* from, const bool outside) const {
    const auto is_it = [&](const command_declaration& candidate) -> bool {
      return candidate.name == name && std::ranges::find(kinds, candidate.kind) != kinds.end();
    };
    for (const scope* at = from; at != nullptr; at = at->outer) {
      for (const command_declaration& part : at->declaration->parts) {
        if (is_it(part)) {
          return &part;
        }
      }
    }
    for (size_t i = 0; outside && i < top_level.size(); ++i) {
      if (is_it(*top_level[i])) {
        return top_level[i];
      }
    }
    return nullptr;
  }

  void command_declaration_checker::error(const vm_error_code code, const source_span& span, std::string message) {
    failed = true;
    diagnostics->emit({ .severity = VM_DIAGNOSTIC_ERROR, .error_code = code, .phase = VM_PHASE_SEMA, .span = span, .final_message = std::move(message) });
  }

  void command_declaration_checker::check_enum(const command_enum& checked) {
    ++counted.enums;
    if (is_plain_type(checked.name)) {
      error(SEMA_DUPLICATE_NAME, checked.span, std::format("'{}' is a type the language has already", checked.name));
    } else if (find_enum(checked.name) != &checked || find_struct(checked.name) != nullptr) {
      error(SEMA_DUPLICATE_NAME, checked.span, std::format("'{}' is declared more than once in this build", checked.name));
    }

    if (checked.values.empty()) {
      error(SEMA_BAD_DECLARATION, checked.span, std::format("enum '{}' has no values", checked.name));
    }
    for (size_t i = 0; i < checked.values.size(); ++i) {
      if (std::ranges::find(checked.values.begin(), checked.values.begin() + static_cast<ptrdiff_t>(i), checked.values[i]) != checked.values.begin() + static_cast<ptrdiff_t>(i)) {
        error(SEMA_DUPLICATE_NAME, checked.span, std::format("enum '{}' has '{}' twice", checked.name, checked.values[i]));
      }
    }
  }

  void command_declaration_checker::check_struct(const command_struct& checked) {
    ++counted.structs;
    if (is_plain_type(checked.name)) {
      error(SEMA_DUPLICATE_NAME, checked.span, std::format("'{}' is a type the language has already", checked.name));
    } else if (find_struct(checked.name) != &checked || find_enum(checked.name) != nullptr) {
      error(SEMA_DUPLICATE_NAME, checked.span, std::format("'{}' is declared more than once in this build", checked.name));
    }

    for (size_t i = 0; i < checked.fields.size(); ++i) {
      const command_struct_field& field = checked.fields[i];
      if (std::ranges::any_of(checked.fields.begin(), checked.fields.begin() + static_cast<ptrdiff_t>(i), [&](const command_struct_field& earlier) { return earlier.name == field.name; })) {
        error(SEMA_DUPLICATE_NAME, field.span, std::format("struct '{}' has two fields named '{}'", checked.name, field.name));
      }

      const bool of_struct = find_struct(field.type) != nullptr;
      const bool second_list = field.list && of_struct && std::ranges::any_of(checked.fields.begin(), checked.fields.begin() + static_cast<ptrdiff_t>(i), [&](const command_struct_field& earlier) { return earlier.list && earlier.type == field.type; });
      if (second_list) {
        error(SEMA_BAD_DECLARATION, field.span, std::format("struct '{}' has two lists of {}; a {} declared inside it could be in either", checked.name, field.type, field.type));
      }
      if (!is_plain_type(field.type) && find_enum(field.type) == nullptr && !of_struct) {
        error(SEMA_UNKNOWN_NAME, field.span, std::format("'{}' is not a type: it is none of {}, nor an enum or a struct of this build", field.type, kPlainTypeList));
        continue;
      }
      if (!field.initial.has_value()) {
        continue;
      }
      /// what a field of a struct's type names has to be declared where it is used. null is
      ///  no name: the field need not be given
      if (of_struct && !field.list && field.initial->kind == CMD_VALUE_NULL) {
        continue;
      }
      if (of_struct) {
        error(SEMA_BAD_DECLARATION, field.initial->span, std::format("'{}' is of the struct '{}', so it has nothing it can be when a declaration does not say: it {}", field.name, field.type, field.list ? "is the declarations inside the body" : "names a declaration"));
        continue;
      }
      check_value(checked, field, false, *field.initial, nullptr);
    }

    for (size_t i = 0; i < checked.short_form.size(); ++i) {
      const std::string& name = checked.short_form[i];
      const command_struct_field* field = field_of(checked, name);
      if (field == nullptr) {
        error(SEMA_UNKNOWN_NAME, checked.span, std::format("the short form of struct '{}' gives '{}', which is not one of its fields", checked.name, name));
      } else if (field->list && find_struct(field->type) != nullptr) {
        error(SEMA_BAD_DECLARATION, checked.span, std::format("the short form of struct '{}' cannot give '{}': that is the declarations inside a body", checked.name, name));
      }
      if (std::ranges::find(checked.short_form.begin(), checked.short_form.begin() + static_cast<ptrdiff_t>(i), name) != checked.short_form.begin() + static_cast<ptrdiff_t>(i)) {
        error(SEMA_DUPLICATE_NAME, checked.span, std::format("the short form of struct '{}' gives '{}' twice", checked.name, name));
      }
    }

    for (const std::string& about : checked.names) {
      if (find_struct(about) == nullptr) {
        error(SEMA_UNKNOWN_NAME, checked.span, std::format("struct '{}' names '{}', which is not a struct", checked.name, about));
      }
    }

    if (!checked.graph.empty()) {
      const command_struct_field* field = field_of(checked, checked.graph);
      if (field == nullptr || !field->list || find_struct(field->type) == nullptr) {
        error(SEMA_BAD_DECLARATION, checked.span, std::format("the graph of struct '{}' is '{}', which is not one of its lists of a struct", checked.name, checked.graph));
      }
    }
  }

  void command_declaration_checker::check_declaration(const command_declaration& declaration, const scope* outer, const bool needs_name) {
    ++counted.declarations;
    const command_struct* kind = find_struct(declaration.kind);
    if (kind == nullptr) {
      error(SEMA_UNKNOWN_NAME, declaration.span, std::format("'{}' is not a struct, so nothing can be declared as one", declaration.kind));
      return;
    }

    if (!kind->names.empty()) {
      /// it has no name of its own: it is about the thing it names
      if (declaration.name.empty()) {
        error(SEMA_BAD_DECLARATION, declaration.span, std::format("a {} is named after the {} it is about, and this one names none", kind->name, joined(kind->names, " or ")));
      } else if (find_declared(kind->names, declaration.name, outer) == nullptr) {
        error(SEMA_UNKNOWN_NAME, declaration.name_span, std::format("'{}' is not a {} declared around this {}", declaration.name, joined(kind->names, " or "), kind->name));
      }
    } else if (outer == nullptr && declaration.name.empty() && needs_name) {
      error(SEMA_BAD_DECLARATION, declaration.span, std::format("a {} declared outside everything needs a name, or a variable to be bound to", kind->name));
    }

    if (outer == nullptr && !declaration.name.empty()) {
      const std::string kinds[] = { declaration.kind };
      if (find_declared(kinds, declaration.name, nullptr) != &declaration) {
        error(SEMA_DUPLICATE_NAME, declaration.name_span, std::format("{} is declared more than once in this build", label(declaration)));
      }
    }

    const scope here = { .declaration = &declaration, .outer = outer };
    ostd::vector<const command_struct_field*> given;
    /// given null, which is the same as not given: the struct's default is what it is
    ostd::vector<const command_struct_field*> unsaid;
    const auto give = [&](const command_struct_field& field, const command_value& value) -> void {
      if (std::ranges::find(given, &field) != given.end()) {
        error(SEMA_DUPLICATE_NAME, value.span, std::format("{} is given its '{}' twice", label(declaration), field.name));
        return;
      }
      given.push_back(&field);
      if (value.kind == CMD_VALUE_NULL) {
        unsaid.push_back(&field);
      }
      check_value(*kind, field, false, value, &here);
    };

    if (declaration.short_form && !declaration.positional.empty()) {
      if (kind->short_form.empty()) {
        error(SEMA_BAD_DECLARATION, declaration.positional.front().span, std::format("a {} has no short form: write '{} {} {{ field = value }}'", kind->name, kind->name, declaration.name));
      } else {
        if (declaration.positional.size() > kind->short_form.size()) {
          error(SEMA_BAD_DECLARATION, declaration.positional[kind->short_form.size()].span, std::format("the short form of a {} takes {} value(s), {}, and this gives {}", kind->name, kind->short_form.size(), joined(kind->short_form, " "), declaration.positional.size()));
        }
        for (size_t i = 0; i < std::min(declaration.positional.size(), kind->short_form.size()); ++i) {
          if (const command_struct_field* field = field_of(*kind, kind->short_form[i]); field != nullptr) {
            give(*field, declaration.positional[i]);
          }
        }
      }
    }

    for (const command_field_value& said : declaration.fields) {
      const command_struct_field* field = field_of(*kind, said.name);
      if (field == nullptr) {
        ostd::vector<std::string> names;
        for (const command_struct_field& known : kind->fields) {
          names.push_back(known.name);
        }
        error(SEMA_UNKNOWN_NAME, said.span, std::format("a {} has no field '{}'; its fields are {}", kind->name, said.name, joined(names, ", ")));
      } else if (field->list && find_struct(field->type) != nullptr) {
        error(SEMA_BAD_DECLARATION, said.span, std::format("'{}' of a {} is the {} declarations inside it, and is not given a value", field->name, kind->name, field->type));
      } else {
        give(*field, said.value);
      }
    }

    for (const command_struct_field& field : kind->fields) {
      const bool is_parts = field.list && find_struct(field.type) != nullptr;
      const bool said = std::ranges::find(given, &field) != given.end() && std::ranges::find(unsaid, &field) == unsaid.end();
      if (!field.initial.has_value() && !is_parts && !said) {
        error(SEMA_BAD_DECLARATION, declaration.name_span, std::format("{} does not say its '{}', which has to be said", label(declaration), field.name));
      }
    }

    for (size_t i = 0; i < declaration.parts.size(); ++i) {
      const command_declaration& part = declaration.parts[i];
      const bool held = std::ranges::any_of(kind->fields, [&](const command_struct_field& field) { return field.list && field.type == part.kind; });
      if (!held && find_struct(part.kind) != nullptr) {
        error(SEMA_BAD_DECLARATION, part.span, std::format("a {} holds no {}", kind->name, part.kind));
      }
      const bool repeated = !part.name.empty() && std::ranges::any_of(declaration.parts.begin(), declaration.parts.begin() + static_cast<ptrdiff_t>(i), [&](const command_declaration& earlier) { return earlier.kind == part.kind && earlier.name == part.name; });
      if (repeated) {
        error(SEMA_DUPLICATE_NAME, part.name_span, std::format("{} has two of {}", label(declaration), label(part)));
      }
      check_declaration(part, &here, false);
    }

    if (!declaration.edges.empty()) {
      check_edges(declaration, *kind);
    }
  }

  void command_declaration_checker::check_edges(const command_declaration& declaration, const command_struct& kind) {
    const command_struct_field* joined_list = kind.graph.empty() ? nullptr : field_of(kind, kind.graph);
    if (joined_list == nullptr) {
      error(SEMA_BAD_DECLARATION, declaration.edges.front().span, std::format("a {} is not a graph: nothing declared in it can be joined with '|>'", kind.name));
      return;
    }

    const auto is_node = [&](const std::string_view name) -> bool {
      return std::ranges::any_of(declaration.parts, [&](const command_declaration& part) { return part.kind == joined_list->type && part.name == name; });
    };

    for (size_t i = 0; i < declaration.edges.size(); ++i) {
      const command_edge& edge = declaration.edges[i];
      bool known = true;
      for (const std::string* end : { &edge.from, &edge.to }) {
        if (!is_node(*end)) {
          error(SEMA_UNKNOWN_NAME, edge.span, std::format("'{}' is not a {} of {}", *end, joined_list->type, label(declaration)));
          known = false;
        }
      }
      if (!known) {
        continue;
      }

      /// this edge closes a ring when where it starts can already be reached from where it
      ///  ends, by the edges before it
      ostd::vector<std::string_view> reached = { edge.to };
      for (size_t next = 0; next < reached.size(); ++next) {
        for (size_t earlier = 0; earlier < i; ++earlier) {
          const command_edge& other = declaration.edges[earlier];
          if (other.from == reached[next] && std::ranges::find(reached, std::string_view{ other.to }) == reached.end()) {
            reached.push_back(other.to);
          }
        }
      }
      if (std::ranges::find(reached, std::string_view{ edge.from }) != reached.end()) {
        error(SEMA_BAD_DECLARATION, edge.span, std::format("'{} |> {}' joins {} in a ring: '{}' already comes after '{}'", edge.from, edge.to, label(declaration), edge.from, edge.to));
      }
    }
  }

  bool command_declaration_checker::check_value(const command_struct& owner, const command_struct_field& field, const bool as_item, const command_value& value, const scope* from) {
    /// null is the field left unsaid, whatever the field's type
    if (value.kind == CMD_VALUE_NULL && !as_item) {
      return true;
    }
    if (field.list && !as_item) {
      if (value.kind != CMD_VALUE_LIST) {
        error(SEMA_BAD_VALUE, value.span, std::format("'{}' of a {} is a list of {}, written [a, b]; this is {}", field.name, owner.name, field.type, describe(value)));
        return false;
      }
      bool fits = true;
      for (const command_value& item : value.items) {
        fits = check_value(owner, field, true, item, from) && fits;
      }
      return fits;
    }

    const auto wrong = [&](const std::string_view wanted) -> bool {
      error(SEMA_BAD_VALUE, value.span, std::format("'{}' of a {} is {}; this is {}", field.name, owner.name, wanted, describe(value)));
      return false;
    };

    if (field.type == "any") {
      return true;
    }
    if (field.type == "int") {
      return value.kind == CMD_VALUE_INTEGER || wrong("a whole number");
    }
    if (field.type == "float") {
      return value.kind == CMD_VALUE_INTEGER || value.kind == CMD_VALUE_DECIMAL || wrong("a number");
    }
    if (field.type == "bool") {
      return value.kind == CMD_VALUE_BOOL || wrong("true or false");
    }
    if (field.type == "text") {
      return value.kind == CMD_VALUE_TEXT || wrong(value.kind == CMD_VALUE_NAME ? "a text, which goes in quotes" : "a text");
    }
    if (field.type == "name") {
      return value.kind == CMD_VALUE_NAME || wrong(value.kind == CMD_VALUE_TEXT ? "a name, which is written without quotes" : "a name");
    }
    if (field.type == "asset") {
      return (value.kind == CMD_VALUE_TEXT && !value.text.empty()) || wrong("another asset, by the path of its file in quotes");
    }

    if (const command_enum* choices = find_enum(field.type); choices != nullptr) {
      if (value.kind == CMD_VALUE_NAME && std::ranges::find(choices->values, value.text) != choices->values.end()) {
        return true;
      }
      error(SEMA_BAD_VALUE, value.span, std::format("'{}' of a {} is one of {}; this is {}{}", field.name, owner.name, joined(choices->values, ", "), describe(value), value.kind == CMD_VALUE_NAME ? std::format(", '{}'", value.text) : ""));
      return false;
    }

    /// a field of a struct's type names one that is declared
    if (value.kind != CMD_VALUE_NAME) {
      return wrong(std::format("the name of a {}", field.type));
    }
    const std::string kinds[] = { field.type };
    if (find_declared(kinds, value.text, from) == nullptr) {
      error(SEMA_UNKNOWN_NAME, value.span, std::format("'{}' of a {} names a {}, and no {} '{}' is declared here", field.name, owner.name, field.type, field.type, value.text));
      return false;
    }
    return true;
  }

}  // namespace other
