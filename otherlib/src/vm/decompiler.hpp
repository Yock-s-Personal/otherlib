/**
 * \file vm/decompiler.hpp
 **/
#ifndef OTHERLIB_VM_DECOMPILER_HPP
#define OTHERLIB_VM_DECOMPILER_HPP

#include <cstdint>
#include <functional>
#include <span>
#include <string>

#include "core/defines.hpp"

namespace other {

  struct other_command_device;
  class command_bus;

  struct disassembly {
    std::string text;
    /// why the bytes could not be read as a program; empty when text holds a listing
    std::string error;
  };

  /// the text of one line of a source file, asked for by the name the file was linked under;
  ///  empty when the file or the line cannot be had
  using source_line_reader = std::function<opt<std::string>(std::string_view unit, uint32_t line)>;

  struct decompiler {
    static std::string register_byte_name(uint8_t reg);
    static std::string get_instruction_name(uint32_t opcode);
    static std::string opcode_to_string(uint32_t opcode);
    static std::string opcode_to_detailed_string(uint32_t opcode);
    static void hexdump_memory(other_command_device* device);

    static void dump_instructions(const std::span<const uint8_t> instructions);

    /// a linked program, file header included, as text: its instructions in the assembler's
    ///  own words, then its data, then its assets as the asset blocks they were linked from.
    ///  bus names the syscalls; without one they are numbers.
    ///  read_line, when given, puts each source line above the instructions made from it
    static disassembly disassemble(std::span<const uint8_t> bytes, const command_bus* bus = nullptr, const source_line_reader& read_line = {});

    /// the data of a linked program, field by field: each with its type, its value read as that
    ///  type, and every instruction that uses it. without a debug section the fields are worked
    ///  out from the addresses the code names, and what they hold is a guess
    static disassembly list_data(std::span<const uint8_t> bytes, const source_line_reader& read_line = {});
  };

}  // namespace other

#endif  // OTHERLIB_VM_DECOMPILER_HPP