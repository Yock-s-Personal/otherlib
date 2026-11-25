/**
 * \file vm/command_language/command_assets.cpp
 **/
#include "vm/command_language/command_assets.hpp"

#include "core/profiler.hpp"

namespace other {
  namespace {

    /// a number given where the struct says float is a float, so that what is written does
    ///  not depend on whether the point was typed
    asset_value value_of(const command_value& value, const bool as_float) {
      switch (value.kind) {
        case CMD_VALUE_INTEGER:
          if (as_float) {
            return { .kind = ASSET_VALUE_DECIMAL, .decimal = static_cast<double>(value.integer) };
          }
          return { .kind = ASSET_VALUE_INTEGER, .integer = value.integer };
        case CMD_VALUE_DECIMAL: return { .kind = ASSET_VALUE_DECIMAL, .decimal = value.decimal };
        case CMD_VALUE_BOOL: return { .kind = ASSET_VALUE_BOOL, .integer = value.integer };
        case CMD_VALUE_TEXT: return { .kind = ASSET_VALUE_TEXT, .text = value.text };
        case CMD_VALUE_NAME: return { .kind = ASSET_VALUE_WORD, .text = value.text };
        case CMD_VALUE_LIST: {
          asset_value list = { .kind = ASSET_VALUE_LIST };
          for (const command_value& item : value.items) {
            list.items.push_back(value_of(item, as_float));
          }
          return list;
        }
        default: return {};
      }
    }

    /// what a declaration gives a field, or nothing when it leaves the field to its default
    const command_value* given(const command_declaration& declaration, const command_struct& kind, const command_struct_field& field) {
      for (size_t i = 0; i < std::min(declaration.positional.size(), kind.short_form.size()); ++i) {
        if (kind.short_form[i] == field.name) {
          return &declaration.positional[i];
        }
      }
      const auto found = std::ranges::find(declaration.fields, field.name, &command_field_value::name);
      return found != declaration.fields.end() ? &found->value : nullptr;
    }

    /// a name an asset block can hold: a word, as the assembler reads one
    std::string as_word(const std::string_view name) {
      std::string word{ name };
      for (char& c : word) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_' && c != '-') {
          c = '_';
        }
      }
      const bool starts_as_one = !word.empty() && (std::isalpha(static_cast<unsigned char>(word.front())) != 0 || word.front() == '_');
      return starts_as_one ? word : std::format("_{}", word);
    }

  }  // namespace

  asset_node command_asset_lowering::lower(const command_declaration& declaration, const std::string_view name) const {
    PROFILE_SECTION("command_asset_lowering::lower");
    return lower(declaration, nullptr, name);
  }

  std::string command_asset_lowering::declared_name(const command_declaration& declaration, const std::string_view unit_name) {
    return declaration.name.empty() ? as_word(filepath{ unit_name }.stem().string()) : declaration.name;
  }

  std::string command_asset_lowering::bound_name(const command_stmt& let) {
    OTHER_ASSERT(!let.declared.empty(), "'{}' was not given a declaration", let.name);
    return let.declared.front().name.empty() ? as_word(let.name) : let.declared.front().name;
  }

  asset_node command_asset_lowering::lower(const command_declaration& declaration, const command_declaration_checker::scope* outer, const std::string_view name) const {
    const command_struct* kind = declarations.find_struct(declaration.kind);
    OTHER_ASSERT(kind != nullptr, "'{}' is not a struct; only a build that was checked is lowered", declaration.kind);

    const command_declaration_checker::scope here = { .declaration = &declaration, .outer = outer };
    asset_node node = { .kind = declaration.kind, .name = std::string{ name } };
    for (const command_struct_field& field : kind->fields) {
      const bool of_struct = declarations.find_struct(field.type) != nullptr;
      /// a list of a struct's type is the parts, below
      if (field.list && of_struct) {
        continue;
      }
      const command_value* said = given(declaration, *kind, field);
      const command_value* value = said != nullptr ? said : field.initial.has_value() ? &*field.initial :
                                                                                        nullptr;
      /// given null is not given: the struct's default is what it is
      if (said != nullptr && said->kind == CMD_VALUE_NULL) {
        value = field.initial.has_value() ? &*field.initial : nullptr;
      }
      OTHER_ASSERT(value != nullptr, "'{}' of {} '{}' has no value; only a build that was checked is lowered", field.name, declaration.kind, declaration.name);
      /// left unsaid by the struct too: the block has nothing for it, and whoever reads the asset decides
      if (value->kind == CMD_VALUE_NULL) {
        continue;
      }
      /// another asset, by where its file is from this one
      if (field.type == "asset" && !field.list) {
        node.fields.push_back({ .name = field.name, .how = ASSET_FIELD_USE, .value = { .kind = ASSET_VALUE_TEXT, .text = value->text } });
        continue;
      }

      if (!of_struct) {
        node.fields.push_back({ .name = field.name, .how = ASSET_FIELD_SET, .value = value_of(*value, field.type == "float") });
        continue;
      }
      /// what it names is a part of this asset, or else another asset of the build
      const std::string kinds[] = { field.type };
      const bool inside = declarations.find_declared(kinds, value->text, &here, false) != nullptr;
      node.fields.push_back({ .name = field.name, .how = inside ? ASSET_FIELD_REF : ASSET_FIELD_USE, .value = { .kind = ASSET_VALUE_WORD, .text = value->text } });
    }

    /// the parts by the struct's lists, in the struct's order; each list's as they were written
    for (const command_struct_field& field : kind->fields) {
      if (!field.list || declarations.find_struct(field.type) == nullptr) {
        continue;
      }
      for (const command_declaration& part : declaration.parts) {
        if (part.kind == field.type) {
          node.parts.push_back(lower(part, &here, part.name));
        }
      }
    }
    for (const command_edge& edge : declaration.edges) {
      node.edges.push_back({ .from = edge.from, .to = edge.to });
    }
    return node;
  }

}  // namespace other
