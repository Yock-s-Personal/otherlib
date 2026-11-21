/**
 * \file vm/command_files/ocmd_toolchain.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_FILES_OCMD_TOOLCHAIN_HPP
#define OTHERLIB_VM_COMMAND_FILES_OCMD_TOOLCHAIN_HPP

#include "vm/command_files/ocmd_headers.hpp"
#include "vm/command_files/ocmd_program.hpp"
#include "data-structures/std_container.hpp"
#include "vm/vm_version.hpp"

namespace other {

  struct other_command_device;
  class command_bus;
  class diagnostic_engine;

  struct oasm_source {
    /// names the source in errors and decides its place in the linked program
    std::string name;
    std::string text;
  };

  struct obin_build_result {
    ostd::vector<uint8_t> bytes;
    bool success = false;
  };

  class ocmd_toolchain {
   public:
    ostd::vector<uint8_t> assemble_oasm_source(other_command_device* device, const filepath& path);

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