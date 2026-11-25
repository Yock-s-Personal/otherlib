/**
 * \file vm/command_files/ocmd_toolchain.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_FILES_OCMD_TOOLCHAIN_HPP
#define OTHERLIB_VM_COMMAND_FILES_OCMD_TOOLCHAIN_HPP

#include "data-structures/std_container.hpp"

#include "vm/command_files/ocmd_program.hpp"
#include "vm/command_language/command_ast.hpp"
#include "vm/command_language/command_declarations.hpp"
#include "vm/command_language/command_lowering.hpp"

namespace other {

  struct other_command_device;
  class command_bus;
  class diagnostic_engine;

  struct oasm_source {
    /// names the source in errors and decides its place in the linked program
    std::string name;
    std::string text;
    /// for text that was generated: for each of its lines, in order, the place it was made
    ///  from in the file called name. empty when text is that file, and its own lines count
    ostd::vector<source_location> origins = {};
  };

  struct obin_build_result {
    ostd::vector<uint8_t> bytes;
    bool success = false;
  };

  struct command_build_options {
    bool has_entry_unit = false;
    bool name_by_path = false;
    bool strict = false;
  };

  /// one parsed file of the language: a .ocmd file, or a file of declarations
  struct command_unit {
    /// names the file in errors and in the OASM made from it
    std::string name;
    command_file file;
    /// reached through a 'let' given its path: what it declares is that variable
    bool included = false;
  };

  class ocmd_toolchain {
   public:
    ostd::vector<uint8_t> assemble_oasm_source(other_command_device* device, const filepath& path);
    /// a .ocmd file and every file it uses, built into one program with that file as the entry
    ostd::vector<uint8_t> assemble_command_source(other_command_device* device, const filepath& path);

    /// empty when text has errors; they go to diag and, with the file's name, to the log.
    ///  declarations_only: text is a file of declarations and holds nothing that runs
    opt<command_file> parse_command_source(std::string_view name, std::string_view text, diagnostic_engine* diag, bool declarations_only = false);
    /// root and every file it reaches, root first: by 'use', and by a 'let' given the path of
    ///  a file of declarations. empty when one is missing or has errors.
    ///  a file whose name ends in .ocmd is one that runs; any other is a file of declarations.
    ///  the engine's own kinds are read with them when something is declared as a kind none
    ///  of the files has
    ostd::vector<command_unit> read_command_units(const filepath& root, std::string_view root_name, diagnostic_engine* diag);
    /// holds every declaration the units make to its struct; nothing is made. false when one
    ///  has an error. counts, when given, receives what was checked
    bool check_command_units(std::span<const command_unit> units, diagnostic_engine* diag, command_check_counts* counts = nullptr);
    /// one OASM source per unit, ready for build_obin. units[0] is the entry file; empty when any unit fails.
    ///  every declaration outside a block is checked and written as an asset block of its unit
    ostd::vector<oasm_source> lower_command_units(const command_bus& bus, std::span<const command_unit> units, diagnostic_engine* diag, ocmd_link_options options = {});
    /// the same, for a build that chooses what its entry is. hooks, when given, receives every
    ///  'on' handler the units hold
    ostd::vector<oasm_source> lower_command_units(const command_bus& bus, std::span<const command_unit> units, diagnostic_engine* diag, const command_build_options& options,
                                                  ostd::vector<command_hook_record>* hooks);

    /// build_obin in its two halves, for a build that looks at the compiled units before it
    ///  links them. compile_units gives back nothing when a source fails
    ostd::vector<ocmd_unit> compile_units(std::span<const oasm_source> sources, diagnostic_engine* diag);
    obin_build_result link_units(const command_bus& bus, std::span<const ocmd_unit> units, diagnostic_engine* diag, ocmd_link_options options = {});

    /// compiles each source on its own and links them into one program. syscalls resolve
    ///  against the devices on bus, so it must hold what the loading driver registers
    obin_build_result build_obin(const command_bus& bus, std::span<const oasm_source> sources, diagnostic_engine* diag, ocmd_link_options options = {});

    /// why this build will not load bytes as a linked program; empty when it will
    opt<std::string> check_obin(std::span<const uint8_t> bytes) const;
    /// writes nothing when check_obin refuses bytes, so it cannot make a file this build would not load
    bool write_obin(const filepath& path, std::span<const uint8_t> bytes);
    /// empty when the file cannot be read or check_obin refuses it
    ostd::vector<uint8_t> read_obin(const filepath& path);
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_FILES_OCMD_TOOLCHAIN_HPP