/**
 * \file vm/command_files/asset_footprint.hpp
 * an asset as bytes: its footprint. the same asset gives the same bytes in every build, from
 * every folder and beside whatever else is linked with it, so the bytes and their hash stand
 * for the asset. footprints sit in a section of a linked file that is never loaded into the device
 **/
#ifndef OTHERLIB_VM_COMMAND_FILES_ASSET_FOOTPRINT_HPP
#define OTHERLIB_VM_COMMAND_FILES_ASSET_FOOTPRINT_HPP

#include <span>

#include "vm/command_files/asset_block.hpp"

namespace other {

  constexpr inline uint8_t kAssetSectionVersion = 1;

  /// FNV-1a at 64 bits, as it is published. not core's FNV(), which starts from another number,
  ///  reads a char as signed and mixes the length in: a footprint has to mean the same on every
  ///  platform and to every tool that reads it
  constexpr uint64_t asset_hash(const std::string_view text) {
    uint64_t hash = 0xCBF29CE484222325;
    for (const char c : text) {
      hash ^= static_cast<uint8_t>(c);
      hash *= 0x100000001B3;
    }
    return hash;
  }
  static_assert(asset_hash("") == 0xCBF29CE484222325 && asset_hash("a") == 0xAF63DC4C8601EC8C, "asset_hash must be FNV-1a 64");

  uint64_t asset_hash(std::span<const uint8_t> bytes);

#pragma pack(push, 1)
  /// starts the section, which follows the program and its debug section when the file
  ///  header's has_assets_flag is set. after it: asset_count rows, and then the footprints
  struct asset_section_header {
    char signature[4] = { 'O', 'A', 'S', 'T' };
    uint8_t version = kAssetSectionVersion;
    uint8_t reserved[3] = {};
    /// bytes in the whole section, this header included
    uint32_t size = 0;
    uint32_t asset_count = 0;
  };
  static_assert(sizeof(asset_section_header) == 16, "Asset section header size must be 16 bytes");

  /// rows are in the order of kind and then name, so one can be found by halving
  struct asset_table_row {
    /// asset_hash of the kind's name and of the asset's
    uint64_t kind = 0;
    uint64_t name = 0;
    /// where the footprint starts, counted from the section's first byte
    uint32_t offset = 0;
    uint32_t size = 0;
    /// asset_hash of the footprint in the first 8 bytes. the rest is zero, and is room for a
    ///  longer hash
    uint8_t hash[16] = {};
  };
  static_assert(sizeof(asset_table_row) == 40, "Asset table row size must be 40 bytes");
#pragma pack(pop)

  /// a field of a footprint. its name is not in the footprint, only the hash of it
  struct footprint_field {
    uint64_t name = 0;
    asset_field_kind how = ASSET_FIELD_SET;
    asset_value value;
  };

  /// a footprint read back: an asset or a part of one, with the hashes where a footprint has them
  struct footprint_node {
    uint64_t kind = 0;
    std::string name;
    /// in the order of their names, whatever order they were written in
    ostd::vector<footprint_field> fields;
    /// in the order they were written in, which can mean something: a material's parameters
    ///  are laid out in it
    ostd::vector<footprint_node> parts;
    /// in the order of their names, each once
    ostd::vector<asset_edge> edges;

    /// name is asset_hash of the field's name; nullptr when the node has no such field
    const footprint_field* field(uint64_t name) const;
  };

  /// no address, no path of this machine and nothing of the order its fields or edges were
  ///  written in goes into it
  ostd::vector<uint8_t> asset_footprint(const asset_node& asset);
  /// empty when bytes are not one whole footprint that this build reads
  opt<footprint_node> read_asset_footprint(std::span<const uint8_t> bytes);

  /// two fields of one node whose names have the same hash, if there are any: a footprint
  ///  could not tell them apart. empty when the asset can be written
  opt<std::string> asset_footprint_refusal(const asset_node& asset);

  struct obin_asset {
    uint64_t kind = 0;
    uint64_t name = 0;
    /// counted from the section's first byte
    uint32_t offset = 0;
    uint64_t hash = 0;
    /// points into the section it was decoded from
    std::span<const uint8_t> footprint;
  };

  struct obin_assets {
    ostd::vector<obin_asset> assets;

    const obin_asset* find(uint64_t kind, uint64_t name) const;

    /// the section for these assets, whatever order they are given in. no two may share a
    ///  kind and a name
    static ostd::vector<uint8_t> encode(std::span<const asset_node* const> assets);
    /// empty when section is not an asset section this build reads, or a hash in it does not
    ///  match its footprint. the footprints point into section
    static opt<obin_assets> decode(std::span<const uint8_t> section);
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_FILES_ASSET_FOOTPRINT_HPP
