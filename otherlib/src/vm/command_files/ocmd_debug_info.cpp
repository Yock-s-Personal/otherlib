/**
 * \file vm/command_files/ocmd_debug_info.cpp
 **/
#include "vm/command_files/ocmd_debug_info.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

#include "core/profiler.hpp"

#include "vm/command_files/ocmd_headers.hpp"

namespace other {
  namespace {

    template <typename T>
    void append(ostd::vector<uint8_t>& out, const T& value) {
      const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
      out.insert(out.end(), bytes, bytes + sizeof(T));
    }

    /// takes a T off the front of from; false, with from untouched, when there are too few bytes
    template <typename T>
    bool take(std::span<const uint8_t>& from, T& value) {
      if (from.size() < sizeof(T)) {
        return false;
      }
      std::memcpy(&value, from.data(), sizeof(T));
      from = from.subspan(sizeof(T));
      return true;
    }

    opt<ocmd_file_header> read_file_header(const std::span<const uint8_t> file) {
      ocmd_file_header header;
      std::span<const uint8_t> rest = file;
      if (!take(rest, header)) {
        return std::nullopt;
      }
      return header;
    }

  }  // namespace

  std::string_view debug_field_type_name(const debug_field_type type) {
    switch (type) {
      case DEBUG_FIELD_I8: return "byte";
      case DEBUG_FIELD_U8: return "ubyte";
      case DEBUG_FIELD_I16: return "int16";
      case DEBUG_FIELD_U16: return "uint16";
      case DEBUG_FIELD_I32: return "int32";
      case DEBUG_FIELD_U32: return "uint32";
      case DEBUG_FIELD_I64: return "int64";
      case DEBUG_FIELD_U64: return "uint64";
      case DEBUG_FIELD_F32: return "float";
      case DEBUG_FIELD_F64: return "double";
      case DEBUG_FIELD_STRING: return "string";
      case DEBUG_FIELD_ADDRESS: return "address";
      case DEBUG_FIELD_BLOB: return "blob";
      case DEBUG_FIELD_USER_DEFINED: return "user_type";
      default: return "";
    }
  }

  ostd::vector<uint8_t> obin_debug_info::encode() const {
    PROFILE_SECTION("obin_debug_info::encode");
    ostd::vector<uint8_t> payload;
    for (const debug_symbol& symbol : symbols) {
      const size_t name_length = std::min<size_t>(symbol.name.size(), std::numeric_limits<uint16_t>::max());
      append(payload, debug_symbol_record{
                        .kind = static_cast<uint8_t>(symbol.kind),
                        .type = static_cast<uint8_t>(symbol.type),
                        .name_length = static_cast<uint16_t>(name_length),
                        .address = symbol.address,
                        .size = symbol.size,
                        .parent = symbol.parent,
                      });
      payload.insert(payload.end(), symbol.name.begin(), symbol.name.begin() + static_cast<std::ptrdiff_t>(name_length));
    }

    ostd::vector<uint8_t> line_payload;
    for (const debug_line_entry& line : lines) {
      append(line_payload, debug_line_record{
                             .address = line.address,
                             .unit = line.unit,
                             .line = line.line,
                             .column = static_cast<uint16_t>(std::min<uint32_t>(line.column, std::numeric_limits<uint16_t>::max())),
                           });
    }

    /// a program with no line information has no lines chunk, as files written before there was one
    const uint32_t chunk_count = line_payload.empty() ? 1 : 2;
    const size_t size = sizeof(debug_section_header) + chunk_count * sizeof(debug_chunk_header) + payload.size() + line_payload.size();

    ostd::vector<uint8_t> section;
    section.reserve(size);
    append(section, debug_section_header{ .size = static_cast<uint32_t>(size), .chunk_count = chunk_count });
    append(section, debug_chunk_header{ .kind = DEBUG_CHUNK_SYMBOLS, .size = static_cast<uint32_t>(payload.size()) });
    section.insert(section.end(), payload.begin(), payload.end());
    if (!line_payload.empty()) {
      append(section, debug_chunk_header{ .kind = DEBUG_CHUNK_LINES, .size = static_cast<uint32_t>(line_payload.size()) });
      section.insert(section.end(), line_payload.begin(), line_payload.end());
    }
    return section;
  }

  opt<obin_debug_info> obin_debug_info::decode(const std::span<const uint8_t> section) {
    PROFILE_SECTION("obin_debug_info::decode");
    std::span<const uint8_t> rest = section;
    debug_section_header header;
    if (!take(rest, header) || std::string_view{ header.signature, sizeof(header.signature) } != "ODBG" || header.version != kDebugSectionVersion ||
        header.size != section.size()) {
      return std::nullopt;
    }

    obin_debug_info info;
    for (uint32_t i = 0; i < header.chunk_count; ++i) {
      debug_chunk_header chunk;
      if (!take(rest, chunk) || chunk.size > rest.size()) {
        return std::nullopt;
      }
      std::span<const uint8_t> payload = rest.first(chunk.size);
      rest = rest.subspan(chunk.size);
      if (chunk.kind == DEBUG_CHUNK_LINES) {
        if (payload.size() % sizeof(debug_line_record) != 0) {
          return std::nullopt;
        }
        debug_line_record record;
        while (take(payload, record)) {
          info.lines.push_back({ .address = record.address, .unit = record.unit, .line = record.line, .column = record.column });
        }
        continue;
      }
      /// a kind this build does not know was written by a newer one; the rest is still good
      if (chunk.kind != DEBUG_CHUNK_SYMBOLS) {
        continue;
      }

      while (!payload.empty()) {
        debug_symbol_record record;
        if (!take(payload, record) || record.name_length > payload.size() || record.kind >= NUM_DEBUG_SYMBOL_KINDS) {
          return std::nullopt;
        }
        /// a parent is always written before what belongs to it
        if (record.parent != debug_symbol::kNoParent && record.parent >= info.symbols.size()) {
          return std::nullopt;
        }
        info.symbols.push_back({
          .kind = static_cast<debug_symbol_kind>(record.kind),
          .name = std::string{ reinterpret_cast<const char*>(payload.data()), record.name_length },
          .address = record.address,
          .size = record.size,
          .parent = record.parent,
          /// a type this build has no name for was written by a newer one; the field is still good
          .type = record.type < NUM_DEBUG_FIELD_TYPES ? static_cast<debug_field_type>(record.type) : DEBUG_FIELD_UNKNOWN,
        });
        payload = payload.subspan(record.name_length);
      }
    }

    /// checked here and not while reading, so the two chunks may come in either order
    for (size_t i = 0; i < info.lines.size(); ++i) {
      const debug_line_entry& line = info.lines[i];
      const bool names_a_unit = line.unit < info.symbols.size() && info.symbols[line.unit].kind == DEBUG_SYMBOL_UNIT;
      if ((line.line != 0 && !names_a_unit) || (i > 0 && line.address < info.lines[i - 1].address)) {
        return std::nullopt;
      }
    }
    return info;
  }

  const debug_line_entry* obin_debug_info::line_at(const uint32_t address) const {
    /// the last entry at or before address covers it
    const auto after = std::ranges::upper_bound(lines, address, {}, &debug_line_entry::address);
    if (after == lines.begin()) {
      return nullptr;
    }
    const debug_line_entry& covering = *std::prev(after);
    return covering.line != 0 ? &covering : nullptr;
  }

  std::span<const uint8_t> obin_program_bytes(const std::span<const uint8_t> file) {
    const opt<ocmd_file_header> header = read_file_header(file);
    if (!header.has_value()) {
      return {};
    }
    const std::span<const uint8_t> rest = file.subspan(sizeof(ocmd_file_header));
    if (header->prog_header.has_debug_flag == 0) {
      return rest;
    }
    return rest.first(std::min<size_t>(header->prog_header.program_size, rest.size()));
  }

  std::span<const uint8_t> obin_debug_bytes(const std::span<const uint8_t> file) {
    const opt<ocmd_file_header> header = read_file_header(file);
    if (!header.has_value() || header->prog_header.has_debug_flag == 0) {
      return {};
    }
    const std::span<const uint8_t> rest = file.subspan(sizeof(ocmd_file_header));
    return rest.subspan(std::min<size_t>(header->prog_header.program_size, rest.size()));
  }

}  // namespace other
