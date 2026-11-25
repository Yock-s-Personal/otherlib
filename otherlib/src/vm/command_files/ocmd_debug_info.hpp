/**
 * \file vm/command_files/ocmd_debug_info.hpp
 * what a linked program is made of, kept for people and tools: the names the linker would
 * otherwise throw away. it sits after the program bytes and is never loaded into the device
 **/
#ifndef OTHERLIB_VM_COMMAND_FILES_OCMD_DEBUG_INFO_HPP
#define OTHERLIB_VM_COMMAND_FILES_OCMD_DEBUG_INFO_HPP

#include <span>

#include "data-structures/std_container.hpp"

namespace other {

  constexpr inline uint8_t kDebugSectionVersion = 1;

  enum debug_chunk_kind : uint16_t {
    DEBUG_CHUNK_SYMBOLS = 1,
    /// which source line each stretch of code was made from; needs the symbols chunk for its units
    DEBUG_CHUNK_LINES = 2,
    /// what the asset section holds only the hashes of, and where each asset was declared
    DEBUG_CHUNK_ASSETS = 3,
  };

  enum debug_symbol_kind : uint8_t {
    /// a source file. the parent of everything that came from it
    DEBUG_SYMBOL_UNIT,
    /// a code block; its parent is its unit
    DEBUG_SYMBOL_BLOCK,
    /// a jump label; its parent is its block
    DEBUG_SYMBOL_LABEL,
    /// a data block; its parent is its unit
    DEBUG_SYMBOL_DATA,
    /// a field; its parent is its data block
    DEBUG_SYMBOL_FIELD,

    NUM_DEBUG_SYMBOL_KINDS,
  };

  /// what a field was declared as. numbered here and not by the assembler's own enum, so a
  ///  file keeps its meaning if that enum is reordered. 0 is a file written before types were kept
  enum debug_field_type : uint8_t {
    DEBUG_FIELD_UNKNOWN = 0,
    DEBUG_FIELD_I8,
    DEBUG_FIELD_U8,
    DEBUG_FIELD_I16,
    DEBUG_FIELD_U16,
    DEBUG_FIELD_I32,
    DEBUG_FIELD_U32,
    DEBUG_FIELD_I64,
    DEBUG_FIELD_U64,
    DEBUG_FIELD_F32,
    DEBUG_FIELD_F64,
    DEBUG_FIELD_STRING,
    DEBUG_FIELD_ADDRESS,
    DEBUG_FIELD_BLOB,
    DEBUG_FIELD_USER_DEFINED,

    NUM_DEBUG_FIELD_TYPES,
  };

  /// the type as it is written in a .oasm file; empty for DEBUG_FIELD_UNKNOWN
  std::string_view debug_field_type_name(debug_field_type type);

#pragma pack(push, 1)
  /// starts the bytes that follow the program when the file header's has_debug_flag is set
  struct debug_section_header {
    char signature[4] = { 'O', 'D', 'B', 'G' };
    uint8_t version = kDebugSectionVersion;
    uint8_t reserved[3] = {};
    /// bytes in the whole section, this header included
    uint32_t size = 0;
    uint32_t chunk_count = 0;
  };
  static_assert(sizeof(debug_section_header) == 16, "Debug section header size must be 16 bytes");

  /// the section is a list of chunks. a reader skips the kinds it does not know, so a new kind
  ///  of information is a new chunk and the ones already written stay as they are
  struct debug_chunk_header {
    uint16_t kind = 0;
    uint16_t reserved = 0;
    /// bytes of payload after this header
    uint32_t size = 0;
  };
  static_assert(sizeof(debug_chunk_header) == 8, "Debug chunk header size must be 8 bytes");

  /// one entry of a DEBUG_CHUNK_SYMBOLS payload; name_length bytes of name follow it
  struct debug_symbol_record {
    uint8_t kind = 0;
    /// a debug_field_type, for a field; 0 for every other kind
    uint8_t type = 0;
    uint16_t name_length = 0;
    uint32_t address = 0;
    uint32_t size = 0;
    uint32_t parent = 0;
  };
  static_assert(sizeof(debug_symbol_record) == 16, "Debug symbol record size must be 16 bytes");

  /// one entry of a DEBUG_CHUNK_LINES payload
  struct debug_line_record {
    uint32_t address = 0;
    uint32_t unit = 0;
    uint32_t line = 0;
    uint16_t column = 0;
    uint16_t reserved = 0;
  };
  static_assert(sizeof(debug_line_record) == 16, "Debug line record size must be 16 bytes");

  /// a DEBUG_CHUNK_ASSETS payload is a count of words, each word as a 16-bit length and its
  ///  bytes, and then one of these for each asset
  struct debug_asset_record {
    uint64_t kind = 0;
    uint64_t name = 0;
    uint32_t unit = 0;
    uint32_t line = 0;
    uint16_t column = 0;
    uint16_t reserved = 0;
  };
  static_assert(sizeof(debug_asset_record) == 28, "Debug asset record size must be 28 bytes");
#pragma pack(pop)

  struct debug_symbol {
    constexpr static inline uint32_t kNoParent = 0xFFFFFFFF;

    debug_symbol_kind kind = DEBUG_SYMBOL_UNIT;
    std::string name;
    /// counted from the first byte of the program, as every address inside it is
    uint32_t address = 0;
    uint32_t size = 0;
    /// index of the symbol this one belongs to, always an earlier one; kNoParent for a unit
    uint32_t parent = kNoParent;
    /// for a field, what it was declared as
    debug_field_type type = DEBUG_FIELD_UNKNOWN;
  };

  /// from address up to the next entry's address, the code was made from this place in a source
  ///  file. line 0 says the code from here on has no source line: the assembler added it
  struct debug_line_entry {
    uint32_t address = 0;
    /// index of the unit's symbol; kNoParent when line is 0
    uint32_t unit = debug_symbol::kNoParent;
    /// both count from 1, as an editor shows them
    uint32_t line = 0;
    uint32_t column = 0;
  };

  /// where an asset of the asset section came from. the asset is the one whose table row has
  ///  the same two hashes
  struct debug_asset {
    uint64_t kind = 0;
    uint64_t name = 0;
    /// index of the unit's symbol; kNoParent when it is not known
    uint32_t unit = debug_symbol::kNoParent;
    /// where its declaration starts, counted from 1; line 0 when it has no source line
    uint32_t line = 0;
    uint32_t column = 0;
  };

  struct obin_debug_info {
    ostd::vector<debug_symbol> symbols;
    /// in address order
    ostd::vector<debug_line_entry> lines;
    /// the names of the kinds and of the fields in the asset section, which holds their hashes
    ostd::vector<std::string> asset_words;
    ostd::vector<debug_asset> assets;

    /// the source line the instruction at address was made from; nullptr when it has none
    const debug_line_entry* line_at(uint32_t address) const;

    /// a whole debug section, ready to follow the program bytes
    ostd::vector<uint8_t> encode() const;
    /// empty when section is not a debug section this build reads
    static opt<obin_debug_info> decode(std::span<const uint8_t> section);
  };

  /// the part of a linked file that is loaded into the device. with neither flag in the
  ///  header that is everything after the header, as it was before there was a debug section
  std::span<const uint8_t> obin_program_bytes(std::span<const uint8_t> file);
  /// the debug section, which follows the program; empty when the header says there is none
  std::span<const uint8_t> obin_debug_bytes(std::span<const uint8_t> file);
  /// the asset section, which follows both; empty when the header says there is none
  std::span<const uint8_t> obin_asset_bytes(std::span<const uint8_t> file);

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_FILES_OCMD_DEBUG_INFO_HPP
