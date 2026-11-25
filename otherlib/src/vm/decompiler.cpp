/**
 * \file vm/decompiler.cpp
 **/
#include "vm/decompiler.hpp"

#include <sstream>
#include <string>

#include "core/enum_formatter.hpp"
#include "core/logger.hpp"
#include "core/profiler.hpp"
#include "data-structures/std_container.hpp"

#include "vm/command_bus.hpp"
#include "vm/command_files/asset_footprint.hpp"
#include "vm/command_files/ocmd_debug_info.hpp"
#include "vm/command_files/ocmd_headers.hpp"
#include "vm/control_table.hpp"
#include "vm/opcode.hpp"
#include "vm/other_device.hpp"
#include "vm/register.hpp"
#include "vm/vm.hpp"

namespace other {
  namespace {

    constexpr size_t kDataRowSize = 8;
    constexpr size_t kTextPreviewLength = 32;

    using label_map = ostd::unordered_map<uint16_t, std::string>;

    /// how an address is written in the listing. the names come from the debug section when
    ///  the file has one; without it code gets made-up labels and data stays a number
    struct name_table {
      label_map code;
      label_map data;
      /// a field's size, for the fields the debug section names
      ostd::unordered_map<uint16_t, uint32_t> field_sizes;
    };

    std::string data_operand(const name_table& names, const uint16_t address) {
      const auto name = names.data.find(address);
      return name != names.data.end() ? name->second : std::format("{:#06x}", address);
    }

    std::string register_name(const uint8_t index) {
      if (index <= vm_register_idx::VM_RF) {
        return std::format("r{:x}", index);
      }
      return index == vm_register_idx::VM_RFLAG ? std::string{ "rflag" } : std::format("r?{:02x}", index);
    }

    std::string syscall_name(const command_bus* bus, const uint16_t id) {
      const syscall call{ id };
      const command_device* device = bus != nullptr ? bus->find_device(call.device) : nullptr;
      if (device != nullptr) {
        for (const function_descriptor& function : device->functions()) {
          if (function.id == call.function) {
            return std::format("{}.{}", device->device_name(), function.name);
          }
        }
      }
      return std::format("{:#06x}", id);
    }

    bool is_printable(const uint8_t byte) {
      return byte >= ' ' && byte <= '~';
    }

    /// a field's bytes as what they most likely are. the type it was declared with is gone by
    ///  link time, so this goes by looks: all printable is text, up to 8 bytes is a number
    std::string field_value(const std::span<const uint8_t> bytes) {
      constexpr size_t kMaxShownBytes = 16;
      if (bytes.empty()) {
        return "(empty)";
      }
      if (std::ranges::all_of(bytes, is_printable)) {
        return std::format("\"{}\"", std::string_view{ reinterpret_cast<const char*>(bytes.data()), bytes.size() });
      }
      if (bytes.size() <= sizeof(uint64_t)) {
        uint64_t value = 0;
        std::memcpy(&value, bytes.data(), bytes.size());
        return std::format("{}", value);
      }

      std::string hex;
      for (const uint8_t byte : bytes.first(std::min(bytes.size(), kMaxShownBytes))) {
        hex += std::format("{:02x} ", byte);
      }
      return bytes.size() > kMaxShownBytes ? std::format("{}...", hex) : hex;
    }

    /// text as it would be written in a script, so a line break or a quote inside it stays visible
    std::string quoted_text(const std::span<const uint8_t> bytes) {
      std::string text = "\"";
      for (const uint8_t byte : bytes) {
        switch (byte) {
          case '\n': text += "\\n"; break;
          case '\t': text += "\\t"; break;
          case '\0': text += "\\0"; break;
          case '"': text += "\\\""; break;
          case '\\': text += "\\\\"; break;
          default: text += is_printable(byte) ? std::string(1, static_cast<char>(byte)) : std::format("\\x{:02x}", byte); break;
        }
      }
      return text + "\"";
    }

    std::string hex_bytes(const std::span<const uint8_t> bytes) {
      constexpr size_t kMaxShownBytes = 32;
      std::string hex;
      for (const uint8_t byte : bytes.first(std::min(bytes.size(), kMaxShownBytes))) {
        hex += std::format("{:02x} ", byte);
      }
      return bytes.size() > kMaxShownBytes ? std::format("{}... ({} bytes)", hex, bytes.size()) : hex;
    }

    /// a number, with its hex form beside it once it is too large to read at a glance
    std::string number_text(const uint64_t value, const bool negative = false) {
      if (negative) {
        return std::format("{}", static_cast<int64_t>(value));
      }
      return value > 0xFFFF ? std::format("{}  ({:#x})", value, value) : std::format("{}", value);
    }

    /// a field's bytes read as the type it was declared with
    std::string typed_value(const std::span<const uint8_t> bytes, const debug_field_type type) {
      if (bytes.empty()) {
        return "(empty)";
      }
      uint64_t raw = 0;
      std::memcpy(&raw, bytes.data(), std::min(bytes.size(), sizeof(raw)));

      switch (type) {
        case DEBUG_FIELD_I8:
        case DEBUG_FIELD_I16:
        case DEBUG_FIELD_I32:
        case DEBUG_FIELD_I64: {
          /// the sign is in the top bit of what was stored, which is not always the width declared
          const size_t bits = std::min(bytes.size(), sizeof(raw)) * 8;
          const int64_t value = bits == 64 ? static_cast<int64_t>(raw) : static_cast<int64_t>(raw << (64 - bits)) >> (64 - bits);
          return number_text(static_cast<uint64_t>(value), value < 0);
        }
        case DEBUG_FIELD_U8:
        case DEBUG_FIELD_U16:
        case DEBUG_FIELD_U32:
        case DEBUG_FIELD_U64: return number_text(raw);

        case DEBUG_FIELD_F32:
          if (bytes.size() >= sizeof(float)) {
            float value = 0.f;
            std::memcpy(&value, bytes.data(), sizeof(value));
            return std::format("{}", value);
          }
          break;
        case DEBUG_FIELD_F64:
          if (bytes.size() >= sizeof(double)) {
            double value = 0.;
            std::memcpy(&value, bytes.data(), sizeof(value));
            return std::format("{}", value);
          }
          break;

        case DEBUG_FIELD_STRING: return quoted_text(bytes);
        case DEBUG_FIELD_ADDRESS: return std::format("{:#06x}", raw);
        case DEBUG_FIELD_BLOB:
        case DEBUG_FIELD_USER_DEFINED: return hex_bytes(bytes);
        default: break;
      }
      return field_value(bytes);
    }

    /// for a program with no debug section: what a stretch of data most likely is. its end is
    ///  only where the next field starts, so the padding after a text is still on it
    std::string guessed_value(std::span<const uint8_t> bytes, const bool taken_as_address) {
      std::span<const uint8_t> trimmed = bytes;
      while (!trimmed.empty() && trimmed.back() == 0) {
        trimmed = trimmed.first(trimmed.size() - 1);
      }
      /// code that takes a field's address and not its value is handing a text to something
      const bool reads_as_text = !trimmed.empty() && std::ranges::all_of(trimmed, is_printable);
      if (reads_as_text && (taken_as_address || trimmed.size() > sizeof(uint64_t))) {
        return quoted_text(trimmed);
      }
      if (bytes.size() <= sizeof(uint64_t)) {
        uint64_t value = 0;
        std::memcpy(&value, bytes.data(), bytes.size());
        return number_text(value);
      }
      return hex_bytes(bytes);
    }

    struct program_view {
      ocmd_file_header file_header;
      /// what is loaded into the device; every address counts from its first byte
      std::span<const uint8_t> program;
      size_t code_begin = 0;
      size_t code_end = 0;
    };

    /// a footprint as the asset block it was linked from. a footprint holds the hash of a
    ///  kind's name and of a field's; words has the names, when the file has a debug section
    asset_node block_of(const footprint_node& node, const ostd::unordered_map<uint64_t, std::string>& words) {
      const auto word = [&words](const uint64_t hash) -> std::string {
        const auto found = words.find(hash);
        return found != words.end() ? found->second : std::format("#{:016x}", hash);
      };

      asset_node block = { .kind = word(node.kind), .name = node.name, .edges = node.edges };
      for (const footprint_field& field : node.fields) {
        block.fields.push_back({ .name = word(field.name), .how = field.how, .value = field.value });
      }
      for (const footprint_node& part : node.parts) {
        block.parts.push_back(block_of(part, words));
      }
      return block;
    }

    /// the assets of a linked file, each as an asset block under where it came from. empty
    ///  when the file has no asset section
    std::string assets_text(const std::span<const uint8_t> file, const opt<obin_debug_info>& debug) {
      const std::span<const uint8_t> section = obin_asset_bytes(file);
      if (section.empty()) {
        return {};
      }
      std::string text = "\nassets\n";
      const opt<obin_assets> assets = obin_assets::decode(section);
      if (!assets.has_value()) {
        text += "  the asset section could not be read\n";
        return text;
      }

      ostd::unordered_map<uint64_t, std::string> words;
      for (size_t i = 0; debug.has_value() && i < debug->asset_words.size(); ++i) {
        words[asset_hash(debug->asset_words[i])] = debug->asset_words[i];
      }
      if (words.empty()) {
        text += "; no debug section to name them: a kind and a field are the hash of their names\n";
      }

      for (const obin_asset& asset : assets->assets) {
        text += "\n";
        const debug_asset* from = nullptr;
        for (size_t i = 0; debug.has_value() && i < debug->assets.size(); ++i) {
          if (debug->assets[i].kind == asset.kind && debug->assets[i].name == asset.name) {
            from = &debug->assets[i];
          }
        }
        if (from != nullptr && from->unit != debug_symbol::kNoParent) {
          const std::string& unit = debug->symbols[from->unit].name;
          text += from->line != 0 ? std::format("; {}:{}:{}\n", unit, from->line, from->column) : std::format("; {}\n", unit);
        }
        text += std::format("; footprint {:#06x} to {:#06x} of the asset section, {} bytes, hash {:016x}\n", asset.offset, asset.offset + asset.footprint.size(), asset.footprint.size(), asset.hash);

        const opt<footprint_node> footprint = read_asset_footprint(asset.footprint);
        if (!footprint.has_value()) {
          text += "; this footprint could not be read\n";
          continue;
        }
        text += asset_block_text(block_of(*footprint, words));
      }
      return text;
    }

    /// fills view from a linked file; gives back why it could not, or nothing when it could
    std::string open_program(const std::span<const uint8_t> bytes, program_view& view) {
      if (bytes.size() < sizeof(ocmd_file_header)) {
        return std::format("{} bytes is smaller than the {}-byte header", bytes.size(), sizeof(ocmd_file_header));
      }
      std::memcpy(&view.file_header, bytes.data(), sizeof(view.file_header));
      if (std::string_view{ view.file_header.file_signature, sizeof(view.file_header.file_signature) } != "OCMD") {
        return "the file signature is not 'OCMD'";
      }
      view.program = obin_program_bytes(bytes);
      view.code_begin = view.file_header.prog_header.code_section_offset;
      view.code_end = view.file_header.prog_header.data_section_offset;
      if (view.code_begin > view.code_end || view.code_end > view.program.size() || (view.code_end - view.code_begin) % sizeof(instruction) != 0) {
        return std::format("the header puts the code at {:#06x} to {:#06x} in a program of {} bytes", view.code_begin, view.code_end, view.program.size());
      }
      return {};
    }

    /// category in the high nibble, type in the low one: the first byte of the instruction as written
    uint8_t instruction_kind(const instruction& instr) {
      return static_cast<uint8_t>((instr.category_nibble() << 4) | instr.type_nibble());
    }

    /// the instruction as it would be written in a .oasm file, with addresses in place of names
    std::string instruction_text(const instruction& instr, const command_bus* bus, const name_table& names) {
      const std::string x = register_name(instr.bytes[instruction::X_REGISTER_BYTE_IDX]);
      const std::string y = register_name(instr.bytes[instruction::Y_REGISTER_BYTE_IDX]);
      const std::string z = register_name(instr.bytes[instruction::Z_REGISTER_BYTE_IDX]);
      const uint16_t n = instr.lower;
      const auto label = names.code.find(n);
      const std::string target = label != names.code.end() ? label->second : std::format("{:#06x}", n);

      constexpr std::string_view kArithmetic[] = { "add", "sub", "mul", "div", "mod" };
      switch (instruction_kind(instr)) {
        case 0x00: return "stopdev";
        case 0x01: return "dump";
        case 0x02: return std::format("dump {}", x);
        case 0x03: return std::format("dump {}, {:#06x}", x, n);
        case 0x04: return "viewstate";
        case 0x05: return "nop";
        case 0x06: return std::format("clear {}, {}", x, y);

        case 0x10: return std::format("mov {}, {}", x, y);
        case 0x11: return std::format("write {}, {}", x, data_operand(names, n));
        case 0x12: return std::format("write {}, {}", x, y);
        case 0x13: return std::format("set {}, {}", x, label != names.code.end() ? label->second : data_operand(names, n));
        case 0x14: return std::format("set {}, [{}]", x, data_operand(names, n));
        case 0x15: return std::format("set {}, {}", x, n);

        case 0x20: return std::format("goto {}", target);
        case 0x21: return std::format("je {}", target);
        case 0x22: return std::format("jne {}", target);
        case 0x23: return std::format("call {}", target);
        case 0x24: return "ret";
        case 0x25: return std::format("ret {}", x);
        case 0x26: return std::format("syscall {}", syscall_name(bus, n));

        case 0x30:
        case 0x31:
        case 0x32:
        case 0x33:
        case 0x34: return std::format("{} {}, {}", kArithmetic[instr.type_nibble()], x, y);
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39: return std::format("{} {}, {}", kArithmetic[instr.type_nibble() - 5], x, n);

        case 0x40: return std::format("cmp {}, {}, {}", x, y, z);
        case 0x41: return std::format("cmpgt {}, {}, {}", x, y, z);
        case 0x42: return std::format("cmplt {}, {}, {}", x, y, z);
        case 0x43: return std::format("not {}, {}", x, y);
        case 0x44: return std::format("and {}, {}, {}", x, y, z);
        case 0x45: return std::format("or {}, {}, {}", x, y, z);
        case 0x46: return std::format("xor {}, {}, {}", x, y, z);
        case 0x47: return std::format("lshift {}, {}", x, y);
        case 0x48: return std::format("rshift {}, {}", x, y);

        default: return "??";
      }
    }

    /// what an address operand points at: the text that starts there, or the number the field
    ///  holds in the file, which is its value until the program writes to it
    std::string operand_note(const instruction& instr, const std::span<const uint8_t> program, const program_header& header, const command_bus* bus,
                             const name_table& names) {
      const uint16_t n = instr.lower;
      /// a named field has a known end; otherwise the text is read until it stops looking like text
      const auto field_size = names.field_sizes.find(n);
      const size_t text_limit = field_size != names.field_sizes.end() ? field_size->second : kTextPreviewLength;
      const bool in_data = n >= header.data_section_offset && n < program.size();
      switch (instruction_kind(instr)) {
        case 0x14:
          if (in_data && n + sizeof(uint64_t) <= program.size()) {
            uint64_t value = 0;
            std::memcpy(&value, program.data() + n, sizeof(value));
            return std::format("; starts as {}", value);
          }
          return in_data ? "" : "; not in the data section";

        case 0x13: {
          if (!in_data) {
            return n < header.data_section_offset ? "; an address in the code" : "; past the end of the program";
          }
          std::string text;
          for (size_t at = n; at < program.size() && text.size() < text_limit && is_printable(program[at]); ++at) {
            text += static_cast<char>(program[at]);
          }
          return text.empty() ? "" : std::format("; \"{}\"", text);
        }

        case 0x26: return bus != nullptr ? std::format("; id {:#06x}", n) : "";
        default: return "";
      }
    }

  }  // namespace

  namespace detail {

    uint8_t get_register_byte(const instruction& instr) {
      return instr.bytes[instruction::X_REGISTER_BYTE_IDX];
    }

  }  // namespace detail

  std::string decompiler::register_byte_name(uint8_t reg) {
    switch (reg) {
      case instruction::X_REGISTER_BYTE_IDX: return "X";
      case instruction::Y_REGISTER_BYTE_IDX: return "Y";
      case instruction::Z_REGISTER_BYTE_IDX: return "Z";
      default: return "Unknown";
    }
  }

  std::string decompiler::get_instruction_name(uint32_t opcode) {
    instruction instr(opcode);

    std::string res;
    switch (instr.category_nibble()) {
      case 0: res += "Control"; break;
      case 1: res += "Memory/Logic"; break;
      case 2: res += "Program Flow"; break;
      case 3: res += "Arithmetic"; break;
      case 4: res += "Logic and Bitwise"; break;
      default: return res;
    }

    res += ": ";
    switch (instr.category_nibble()) {
      case 0: {
        switch (instr.type_nibble()) {
          case 0x00: res += "Stop Device"; break;
          case 0x01: res += "Dump Registers"; break;
          case 0x02: res += "Dump Register X"; break;
          case 0x03: res += "Dump reg(X) bytes at N"; break;
          case 0x04: res += "View VM Device State"; break;
          case 0x05: res += "No Op"; break;
          case 0x06: res += "Clear X through Y"; break;
          default:
            OTHER_ASSERT(false, "Invalid type nibble : category = {}, type = {}", instr.category_nibble(), instr.type_nibble());
            break;
        }
      } break;
      case 1: {
        switch (instr.type_nibble()) {
          case 0x00: res += "Move X to Y"; break;
          case 0x01: res += "Write X to memory at N"; break;
          case 0x02: res += "Write X to address in Y"; break;
          case 0x03: res += "Set X to address N"; break;
          case 0x04: res += "Set X to dword at N"; break;
          case 0x05: res += "Set X to K"; break;
          default:
            OTHER_ASSERT(false, "Invalid type nibble : category = {}, type = {}", instr.category_nibble(), instr.type_nibble());
            break;
        }
      } break;
      case 2: {
        switch (instr.type_nibble()) {
          case 0x00: res += "Goto address N"; break;
          case 0x01: res += "Jump to N if FLAG == 0"; break;
          case 0x02: res += "Jump to N if FLAG != 0"; break;
          case 0x03: res += "Call address N"; break;
          case 0x04: res += "Return"; break;
          case 0x05: res += "Return storing result in X"; break;
          case 0x06: res += "System call with id K"; break;
          default:
            OTHER_ASSERT(false, "Invalid type nibble : category = {}, type = {}", instr.category_nibble(), instr.type_nibble());
            break;
        }
      } break;
      case 3: {
        switch (instr.type_nibble()) {
          case 0x00: res += "Set X = X + Y"; break;
          case 0x01: res += "Set X = X - Y"; break;
          case 0x02: res += "Set X = X * Y"; break;
          case 0x03: res += "Set X = X / Y"; break;
          case 0x04: res += "Set X = X % Y"; break;
          case 0x05: res += "Set X = X + K"; break;
          case 0x06: res += "Set X = X - K"; break;
          case 0x07: res += "Set X = X * K"; break;
          case 0x08: res += "Set X = X / K"; break;
          case 0x09: res += "Set X = X % K"; break;
          default:
            OTHER_ASSERT(false, "Invalid type nibble : category = {}, type = {}", instr.category_nibble(), instr.type_nibble());
            break;
        }
      } break;
      case 4: {
        switch (instr.type_nibble()) {
          case 0x00: res += "Set Z = X == Y"; break;
          case 0x01: res += "Set Z = X > Y"; break;
          case 0x02: res += "Set Z = X < Y"; break;
          case 0x03: res += "Set Y = ~X"; break;
          case 0x04: res += "Set Z = X & Y"; break;
          case 0x05: res += "Set Z = X | Y"; break;
          case 0x06: res += "Set Z = X ^ Y"; break;
          case 0x07: res += "Set X = X << Y"; break;
          case 0x08: res += "Set X = X >> Y"; break;
          default:
            OTHER_ASSERT(false, "Invalid type nibble : category = {}, type = {}", instr.category_nibble(), instr.type_nibble());
            break;
        }
      } break;
      default:
        OTHER_ASSERT(false, "Invalid category nibble : category = {}", instr.category_nibble());
    }

    return res;
  }

  std::string decompiler::opcode_to_string(uint32_t opcode) {
    instruction instr(opcode);
    return "Opcode(Category: " + std::to_string(instr.category_nibble()) + ", Type: " + std::to_string(instr.type_nibble()) + ")";
  }

  std::string decompiler::opcode_to_detailed_string(uint32_t opcode) {
    instruction instr(opcode);
    std::string result = std::format("Opcode [{}]: ", get_instruction_name(opcode));
    result += std::format("{:#010x}", instr.opcode);
    result += " {\n";
    result += "  Category: " + std::to_string(instr.category_nibble()) + ",\n";
    result += "  Type: " + std::to_string(instr.type_nibble()) + ",\n";
    result += std::format("  N/K: {:#06x},\n", instr.lower);
    result += "  Registers: [\n    ";
    for (size_t i = 0; i < instruction::INVALID_BYTE_IDX; ++i) {
      result += std::format("{} = {:#02x} ", register_byte_name(i), instr.bytes[i]);
      if (i < instruction::Z_REGISTER_BYTE_IDX) {
        result += ", ";
      }
    }
    result += "\n  ],\n";
    result += "}";

    return result;
  }

  void decompiler::hexdump_memory(other_command_device* device) {
    assert(device != nullptr && "Null device!");
    PROFILE_SECTION("decompiler::hexdump_memory");
    std::stringstream ss;
    for (size_t i = 0; i < other_command_device::kMemorySize; ++i) {
      if (i % 16 == 0) {
        if (i != 0) {
          ss << "\n";
        }
        ss << std::format("[{:#06x}] ", i);
      }
      ss << std::format("{:#04x} ", device->memory->at(i));
    }
    CORE_LOG_DEBUG("{}", ss.str());
  }

  void decompiler::dump_instructions(const std::span<const uint8_t> instructions) {
    PROFILE_SECTION("decompiler::dump_instructions");
    if (instructions.size() % other_command_device::kOpCodeSize != 0) {
      CORE_LOG_ERROR("Instruction size is not a multiple of opcode size!");
      return;
    }

    other_command_device device = {};
    vm::initialize_device(&device);
    vm::load_control_table(&device, OTHER_CONTROL_TABLE_DECOMPILER_V000);

    OTHER_ASSERT(instructions.size() >= sizeof(ocmd_file_header), "Instructions size is smaller than OCMD file header size!");
    const ocmd_file_header& file_header = *(reinterpret_cast<const ocmd_file_header*>(instructions.data()));
    const program_header& prog_header = file_header.prog_header;

    /// the header holds no count: the code section is whole instructions, its closing stop included
    OTHER_ASSERT(prog_header.data_section_offset >= prog_header.code_section_offset, "Code section ends before it starts!");
    natural_t num_instructions = (prog_header.data_section_offset - prog_header.code_section_offset) / sizeof(instruction);
    CORE_LOG_DEBUG("OCMD File Version: {}.{}", static_cast<int>(file_header.file_version_major), static_cast<int>(file_header.file_version_minor));
    CORE_LOG_DEBUG("Number of Instructions: {}", num_instructions);
    CORE_LOG_DEBUG("Entry Point Address: {:#06x}", prog_header.entry_point_address);

    uint32_t offset = prog_header.code_section_offset + sizeof(ocmd_file_header);
    for (natural_t i = 0; i < num_instructions; ++i) {
      const uint32_t* code_ptr = reinterpret_cast<const uint32_t*>(instructions.data() + offset);
      device.current_instruction = *code_ptr;
      device.pc = offset;
      offset += other_command_device::kOpCodeSize;

      uint8_t instr_nib = device.current_instruction.category_nibble();
      (*device.control_table)[instr_nib](&device);
    }

    vm::shutdown_device(&device);
  }

  disassembly decompiler::disassemble(const std::span<const uint8_t> bytes, const command_bus* bus, const source_line_reader& read_line) {
    PROFILE_SECTION("decompiler::disassemble");
    if (bytes.size() < sizeof(ocmd_file_header)) {
      return { .error = std::format("{} bytes is smaller than the {}-byte header", bytes.size(), sizeof(ocmd_file_header)) };
    }
    ocmd_file_header file_header;
    std::memcpy(&file_header, bytes.data(), sizeof(file_header));
    if (std::string_view{ file_header.file_signature, sizeof(file_header.file_signature) } != "OCMD") {
      return { .error = "the file signature is not 'OCMD'" };
    }

    /// every address in a program counts from the first byte after the file header
    const std::span<const uint8_t> program = obin_program_bytes(bytes);
    const program_header& header = file_header.prog_header;
    const size_t code_begin = header.code_section_offset;
    const size_t code_end = header.data_section_offset;
    if (code_begin > code_end || code_end > program.size() || (code_end - code_begin) % sizeof(instruction) != 0) {
      return { .error = std::format("the header puts the code at {:#06x} to {:#06x} in a program of {} bytes", code_begin, code_end, program.size()) };
    }

    const std::span<const uint8_t> debug_bytes = obin_debug_bytes(bytes);
    const opt<obin_debug_info> debug = debug_bytes.empty() ? std::nullopt : obin_debug_info::decode(debug_bytes);
    const std::span<const debug_symbol> symbols = debug.has_value() ? std::span<const debug_symbol>{ debug->symbols } : std::span<const debug_symbol>{};

    const auto read_instruction = [&program](const size_t address) -> instruction {
      uint32_t opcode = 0;
      std::memcpy(&opcode, program.data() + address, sizeof(opcode));
      return instruction{ opcode };
    };

    /// first pass: name every address something jumps to or calls, as a label would in the source
    name_table names;
    for (size_t address = code_begin; address < code_end; address += sizeof(instruction)) {
      const instruction instr = read_instruction(address);
      const uint8_t kind = instruction_kind(instr);
      if (kind == 0x23) {
        names.code[instr.lower] = std::format("f_{:04x}", instr.lower);
      } else if (kind >= 0x20 && kind <= 0x22 && !names.code.contains(instr.lower)) {
        names.code[instr.lower] = std::format("l_{:04x}", instr.lower);
      }
    }

    /// the real names replace the made-up ones. a block's name wins over a label on its first instruction
    for (const debug_symbol& symbol : symbols) {
      const uint16_t address = static_cast<uint16_t>(symbol.address);
      switch (symbol.kind) {
        case DEBUG_SYMBOL_LABEL: names.code[address] = symbol.name; break;
        case DEBUG_SYMBOL_DATA: names.data.try_emplace(address, symbol.name); break;
        case DEBUG_SYMBOL_FIELD:
          names.data[address] = std::format("{}.{}", symbols[symbol.parent].name, symbol.name);
          names.field_sizes[address] = symbol.size;
          break;
        default: break;
      }
    }
    for (const debug_symbol& symbol : symbols) {
      if (symbol.kind == DEBUG_SYMBOL_BLOCK) {
        names.code[static_cast<uint16_t>(symbol.address)] = symbol.name;
      }
    }

    /// a uint8_t would be formatted as a character
    const auto number = [](const uint8_t value) -> uint32_t {
      return value;
    };

    disassembly result;
    std::string& text = result.text;
    text += std::format("format {}.{}.{}, linked by engine {}.{}.{}\n", number(file_header.file_version_major), number(file_header.file_version_minor),
                        number(file_header.file_version_patch), number(file_header.environment_version_major), number(file_header.environment_version_minor),
                        number(file_header.environment_version_patch));
    text += std::format("code {:#06x} to {:#06x}, {} instructions\n", code_begin, code_end, (code_end - code_begin) / sizeof(instruction));
    text += std::format("data {:#06x} to {:#06x}, {} bytes\n", code_end, program.size(), program.size() - code_end);
    text += std::format("entry {:#06x}\n", header.entry_point_address);
    if (debug.has_value()) {
      text += std::format("debug section: {} bytes, {} names\n", debug_bytes.size(), symbols.size());
    } else {
      text += debug_bytes.empty() ? "no debug section: names are made up from addresses\n" : "the debug section could not be read: names are made up from addresses\n";
    }
    if (debug.has_value() && debug->lines.empty()) {
      text += "the debug section has no source lines\n";
    }
    if (const std::span<const uint8_t> asset_bytes = obin_asset_bytes(bytes); !asset_bytes.empty()) {
      const opt<obin_assets> assets = obin_assets::decode(asset_bytes);
      text += assets.has_value() ? std::format("asset section: {} bytes, {} asset(s), not loaded into the device\n", asset_bytes.size(), assets->assets.size()) : "the asset section could not be read\n";
    }

    /// the source line the last instruction shown came from, so a line is written once, above
    ///  the first instruction made from it
    const debug_line_entry* shown_line = nullptr;

    text += "\ncode\n";
    for (size_t address = code_begin; address < code_end; address += sizeof(instruction)) {
      const instruction instr = read_instruction(address);

      const debug_line_entry* from = debug.has_value() ? debug->line_at(static_cast<uint32_t>(address)) : nullptr;
      /// assembly is what the listing already shows. repeating each of its lines above the
      ///  instruction it became would double the listing, so a block says where it starts instead
      const bool from_assembly = from != nullptr && symbols[from->unit].name.ends_with(".oasm");

      /// what starts here, written as it is in a .oasm file
      std::string headings;
      if (address == header.entry_point_address) {
        headings += "entry:\n";
      }
      for (const debug_symbol& symbol : symbols) {
        if (symbol.address == address && symbol.kind == DEBUG_SYMBOL_BLOCK) {
          headings += from_assembly ? std::format("${}:    ; {}:{}\n", symbol.name, symbols[symbol.parent].name, from->line) : std::format("${}:    ; {}\n", symbol.name, symbols[symbol.parent].name);
        }
      }
      for (const debug_symbol& symbol : symbols) {
        if (symbol.address == address && symbol.kind == DEBUG_SYMBOL_LABEL) {
          headings += std::format("@{}:\n", symbol.name);
        }
      }
      if (const auto made_up = names.code.find(static_cast<uint16_t>(address)); !debug.has_value() && made_up != names.code.end()) {
        headings += std::format("{}:\n", made_up->second);
      }
      if (!headings.empty()) {
        text += std::format("\n{}", headings);
      }

      if (from != shown_line) {
        shown_line = from;
        if (from != nullptr && !from_assembly) {
          const std::string& unit = symbols[from->unit].name;
          const opt<std::string> source = read_line ? read_line(unit, from->line) : std::nullopt;
          const std::string place = std::format("{}:{}:{}", filepath{ unit }.filename().string(), from->line, from->column);
          text += source.has_value() ? std::format("  ; {:<24} {}\n", place, *source) : std::format("  ; {}\n", place);
        }
      }

      const std::string note = operand_note(instr, program, header, bus, names);
      const std::string line = std::format("  {:04x}  {:08x}  {}", address, instr.opcode, instruction_text(instr, bus, names));
      text += note.empty() ? std::format("{}\n", line) : std::format("{:<52}{}\n", line, note);
    }

    text += "\ndata\n";
    if (debug.has_value()) {
      /// by field: the name, the size the source gave it, and its bytes read as what they look like
      for (size_t i = 0; i < symbols.size(); ++i) {
        const debug_symbol& block = symbols[i];
        if (block.kind != DEBUG_SYMBOL_DATA) {
          continue;
        }
        text += std::format("\n#{}    ; {}, {} bytes\n", block.name, symbols[block.parent].name, block.size);
        for (const debug_symbol& field : symbols) {
          if (field.kind != DEBUG_SYMBOL_FIELD || field.parent != i) {
            continue;
          }
          const size_t begin = std::min<size_t>(field.address, program.size());
          const std::span<const uint8_t> value = program.subspan(begin, std::min<size_t>(field.size, program.size() - begin));
          text += std::format("  {:04x}  {:>4}  .{:<24} {}\n", field.address, field.size, field.name, field_value(value));
        }
      }
      text += assets_text(bytes, debug);
      return result;
    }

    for (size_t address = code_end; address < program.size(); address += kDataRowSize) {
      const std::span<const uint8_t> row = program.subspan(address, std::min(kDataRowSize, program.size() - address));
      std::string hex;
      std::string characters;
      for (const uint8_t byte : row) {
        hex += std::format("{:02x} ", byte);
        characters += is_printable(byte) ? static_cast<char>(byte) : '.';
      }
      /// a row is one 8-byte field or a piece of a longer one, so its value as a number is shown too
      std::string as_number;
      if (row.size() == sizeof(uint64_t)) {
        uint64_t value = 0;
        std::memcpy(&value, row.data(), sizeof(value));
        as_number = std::format("{}", value);
      }
      text += std::format("  {:04x}  {:<24} |{:<8}|  {}\n", address, hex, characters, as_number);
    }
    text += assets_text(bytes, debug);
    return result;
  }

  disassembly decompiler::list_data(const std::span<const uint8_t> bytes, const source_line_reader& read_line) {
    PROFILE_SECTION("decompiler::list_data");
    program_view view;
    if (const std::string error = open_program(bytes, view); !error.empty()) {
      return { .error = error };
    }
    const std::span<const uint8_t> program = view.program;

    const std::span<const uint8_t> debug_bytes = obin_debug_bytes(bytes);
    const opt<obin_debug_info> debug = debug_bytes.empty() ? std::nullopt : obin_debug_info::decode(debug_bytes);
    const std::span<const debug_symbol> symbols = debug.has_value() ? std::span<const debug_symbol>{ debug->symbols } : std::span<const debug_symbol>{};

    /// every instruction that names an address in the data, and what it does with it
    struct data_use {
      uint16_t target = 0;
      uint16_t at = 0;
      std::string_view how;
    };
    ostd::vector<data_use> uses;
    for (size_t address = view.code_begin; address < view.code_end; address += sizeof(instruction)) {
      uint32_t opcode = 0;
      std::memcpy(&opcode, program.data() + address, sizeof(opcode));
      const instruction instr{ opcode };
      if (instr.lower < view.code_end || instr.lower >= program.size()) {
        continue;
      }
      switch (instruction_kind(instr)) {
        case 0x03: uses.push_back({ instr.lower, static_cast<uint16_t>(address), "dumped" }); break;
        case 0x11: uses.push_back({ instr.lower, static_cast<uint16_t>(address), "written" }); break;
        case 0x13: uses.push_back({ instr.lower, static_cast<uint16_t>(address), "address" }); break;
        case 0x14: uses.push_back({ instr.lower, static_cast<uint16_t>(address), "read" }); break;
        default: break;
      }
    }

    struct data_field {
      std::string block;
      std::string unit;
      std::string name;
      uint32_t address = 0;
      uint32_t size = 0;
      debug_field_type type = DEBUG_FIELD_UNKNOWN;
    };
    ostd::vector<data_field> fields;
    if (debug.has_value()) {
      for (size_t i = 0; i < symbols.size(); ++i) {
        for (const debug_symbol& field : symbols) {
          if (symbols[i].kind == DEBUG_SYMBOL_DATA && field.kind == DEBUG_SYMBOL_FIELD && field.parent == i) {
            fields.push_back({ symbols[i].name, symbols[symbols[i].parent].name, field.name, field.address, field.size, field.type });
          }
        }
      }
    } else {
      /// no names: a field starts wherever the code points, and runs to where the next one starts
      ostd::vector<uint16_t> starts = { static_cast<uint16_t>(view.code_end) };
      for (const data_use& use : uses) {
        starts.push_back(use.target);
      }
      std::ranges::sort(starts);
      starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
      for (size_t i = 0; i < starts.size() && starts[i] < program.size(); ++i) {
        const size_t end = i + 1 < starts.size() ? starts[i + 1] : program.size();
        fields.push_back({ .name = std::format("data_{:04x}", starts[i]), .address = starts[i], .size = static_cast<uint32_t>(end - starts[i]) });
      }
    }

    /// which block an instruction is in, and which line of which file it was made from
    const auto where = [&](const uint16_t at) -> std::string {
      std::string block;
      for (const debug_symbol& symbol : symbols) {
        if (symbol.kind == DEBUG_SYMBOL_BLOCK && at >= symbol.address && at < symbol.address + symbol.size) {
          block = std::format("${}", symbol.name);
        }
      }
      const debug_line_entry* from = debug.has_value() ? debug->line_at(at) : nullptr;
      if (from == nullptr) {
        return block;
      }
      const std::string& unit = symbols[from->unit].name;
      const opt<std::string> source = read_line ? read_line(unit, from->line) : std::nullopt;
      const std::string place = std::format("{}:{}:{}", filepath{ unit }.filename().string(), from->line, from->column);
      return source.has_value() ? std::format("{:<18} {:<22} {}", block, place, *source) : std::format("{:<18} {}", block, place);
    };

    size_t name_width = 0;
    for (const data_field& field : fields) {
      name_width = std::max(name_width, field.name.size());
    }

    disassembly result;
    std::string& text = result.text;
    text += std::format("data {:#06x} to {:#06x}, {} bytes, {} fields\n", view.code_end, program.size(), program.size() - view.code_end, fields.size());
    if (!debug.has_value()) {
      text += debug_bytes.empty() ? "no debug section: " : "the debug section could not be read: ";
      text += "a field starts where the code points, and what it holds is a guess\n";
    }

    for (size_t i = 0; i < fields.size(); ++i) {
      const data_field& field = fields[i];
      if (!field.block.empty() && (i == 0 || fields[i - 1].block != field.block)) {
        text += std::format("\n#{}    ; {}\n", field.block, field.unit);
      } else if (i == 0) {
        text += "\n";
      }

      /// the padding after a field is counted as part of it, so a use that lands there is not lost
      const size_t begin = std::min<size_t>(field.address, program.size());
      const size_t reach = i + 1 < fields.size() ? std::max<size_t>(fields[i + 1].address, begin) : program.size();
      const std::span<const uint8_t> value = program.subspan(begin, std::min<size_t>(field.size, program.size() - begin));

      bool taken_as_address = false;
      std::string used;
      for (const data_use& use : uses) {
        if (use.target < begin || use.target >= reach) {
          continue;
        }
        taken_as_address = taken_as_address || use.how == "address";
        const std::string offset = use.target != begin ? std::format(" +{}", use.target - begin) : "";
        used += std::format("          {:<8} {:04x}  {}{}\n", use.how, use.at, where(use.at), offset);
      }

      const std::string_view type_name = debug_field_type_name(field.type);
      text += std::format("  {:04x}  .{:<{}}  {:<8} {:>5}  {}\n", field.address, field.name, name_width, type_name.empty() ? "?" : type_name, field.size,
                          debug.has_value() ? typed_value(value, field.type) : guessed_value(value, taken_as_address));
      text += used.empty() ? "          not used by the code\n" : used;
    }
    return result;
  }

}  // namespace other
