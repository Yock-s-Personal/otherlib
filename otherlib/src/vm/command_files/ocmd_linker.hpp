/**
 * \file vm/command_files/ocmd_linker.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_FILES_OCMD_LINKER_HPP
#define OTHERLIB_VM_COMMAND_FILES_OCMD_LINKER_HPP

#include <array>

#include "core/scope.hpp"

#include "vm/command_files/ocmd_debug_info.hpp"
#include "vm/command_files/ocmd_headers.hpp"
#include "vm/command_files/ocmd_program.hpp"
#include "vm/command_files/symbol_resolver.hpp"
#include "vm/diagnostics/error_codes.hpp"

namespace other {

  class diagnostic_engine;

  class ocmd_linker {
   public:
    /// one program, called kSingleUnitName in link errors
    ocmd_linker(const ocmd_program& code, ocmd_link_options options = {})
        : options(options), units{ ocmd_unit{ .name = std::string{ kSingleUnitName }, .program = code } } {}
    /// linked in name order, whatever order they are given in
    ocmd_linker(std::span<const ocmd_unit> units, ocmd_link_options options = {});
    ~ocmd_linker() = default;

    ostd::vector<uint8_t> link(scope<symbol_resolver> resolver, diagnostic_engine* diag);

    constexpr static inline std::string_view kSingleUnitName = "<source>";
    constexpr static inline std::string_view kNaturalEntryName = "__natural_entry";
    constexpr static inline uint16_t kUnresolvedAddress = 0xFFFF;

    constexpr static inline std::array kPredeterminedSymbols = { kNaturalEntryName };
    static bool is_predetermined(const std::string_view name);

    constexpr static inline size_t kHeaderAddressOffset = sizeof(ocmd_file_header);
    inline uint16_t normalize_address(size_t address) const {
      return address + kHeaderAddressOffset;
    }

   private:
    diagnostic_engine* diagnostics;
    ocmd_link_options options;
    ostd::vector<ocmd_unit> units;
    ostd::vector<size_t> block_units;
    /// for each data section, the unit it came from; filled by merge_units like block_units
    ostd::vector<size_t> data_units;

    /// for each block of code, its address in the linked program; filled by write_code
    ostd::vector<uint16_t> block_addresses;
    /// for each data section, its address in the linked program; filled by write_data_sections
    ostd::vector<uint16_t> data_addresses;

    /// address of the stopdev that closes the code section; filled by link
    uint16_t guard_address = 0;
    ostd::vector<std::string> errors;

    /// every unit's blocks and data sections in link order; the passes below work on this
    ocmd_program code;

    struct symbol_address {
      std::string name;
      uint16_t local_address;
    };
    ostd::vector<symbol_address> compiler_symbol_local_addresses;

    inline uint16_t get_code_section_offset() const {
      return sizeof(ocmd_file_header);
    }
    inline uint16_t get_current_linking_address(const std::span<const uint8_t> binary) const {
      return static_cast<uint16_t>(binary.size());
    }
    uint16_t calculate_code_section_offset(size_t index) const;
    uint16_t globablize_offset(uint16_t offset) const;

    void register_symbols(scope<symbol_resolver>& resolver);

    void write_code(scope<symbol_resolver>& resolver, ostd::vector<uint8_t>& binary);
    // void write_generated_code(scope<symbol_resolver>& resolver, ostd::vector<uint8_t>& binary, std::span<const uint8_t> generated_code);
    void write_data_sections(scope<symbol_resolver>& resolver, ostd::vector<uint8_t>& binary);

    void do_final_linking(scope<symbol_resolver>& resolver, ostd::vector<uint8_t>& binary);
    /// every name the link knows, with where it ended up; read after the program is laid out
    obin_debug_info collect_debug_info() const;

    ostd::vector<uint8_t> create_compiler_generated_symbols(scope<symbol_resolver>& resolver);
    void rewrite_instructions(scope<symbol_resolver>& resolver);

    void merge_units(scope<symbol_resolver>& resolver);

    /// records a link error; the link carries on so one run reports every problem it can
    void report(vm_error_code error, std::string message);
    /// a warning in a normal link, an error in a strict one
    void warn(vm_error_code error, std::string message);
    void throw_if_failed() const;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_FILES_OCMD_LINKER_HPP