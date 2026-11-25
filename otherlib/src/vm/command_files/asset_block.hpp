/**
 * \file vm/command_files/asset_block.hpp
 * an asset block of OASM: '%name : kind' to its 'end'. its lines say how the asset is made
 * and are no instructions of the VM, which never steps through them
 **/
#ifndef OTHERLIB_VM_COMMAND_FILES_ASSET_BLOCK_HPP
#define OTHERLIB_VM_COMMAND_FILES_ASSET_BLOCK_HPP

#include "data-structures/std_container.hpp"

namespace other {

  enum asset_value_kind : uint8_t {
    ASSET_VALUE_INTEGER,
    /// written with a point or an exponent, so that it is told from an integer when read back
    ASSET_VALUE_DECIMAL,
    ASSET_VALUE_BOOL,
    /// in quotes
    ASSET_VALUE_TEXT,
    /// a bare word: a value of an enum, or a name the engine knows
    ASSET_VALUE_WORD,
    ASSET_VALUE_LIST,
  };

  struct asset_value {
    asset_value_kind kind = ASSET_VALUE_INTEGER;
    /// INTEGER, and BOOL as 0 or 1
    int64_t integer = 0;
    double decimal = 0.0;
    /// TEXT and WORD
    std::string text;
    ostd::vector<asset_value> items;

    bool operator==(const asset_value&) const = default;
  };

  enum asset_field_kind : uint8_t {
    /// 'set field, value'
    ASSET_FIELD_SET,
    /// 'ref field, name': a part of this asset
    ASSET_FIELD_REF,
    /// 'use field, name' or 'use field, "path"': another asset, by its name in the build or
    ///  by where its file is
    ASSET_FIELD_USE,
  };

  struct asset_field {
    std::string name;
    asset_field_kind how = ASSET_FIELD_SET;
    asset_value value;

    bool operator==(const asset_field&) const = default;
  };

  struct asset_edge {
    std::string from;
    std::string to;

    bool operator==(const asset_edge&) const = default;
  };

  /// an asset, or a part of one: 'begin kind, name' to its 'end'
  struct asset_node {
    std::string kind;
    /// empty for a part that has none
    std::string name;
    ostd::vector<asset_field> fields;
    ostd::vector<asset_node> parts;
    ostd::vector<asset_edge> edges;

    bool operator==(const asset_node&) const = default;
  };

  /// the block as OASM text. what the assembler reads back from it is the same asset
  std::string asset_block_text(const asset_node& asset);

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_FILES_ASSET_BLOCK_HPP
