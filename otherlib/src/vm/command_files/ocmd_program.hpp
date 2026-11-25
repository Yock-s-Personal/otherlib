/**
 * \file vm/command_files/ocmd_program.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_FILES_OCMD_PROGRAM_HPP
#define OTHERLIB_VM_COMMAND_FILES_OCMD_PROGRAM_HPP

#include <string>

#include "vm/command_files/asset_block.hpp"
#include "vm/command_files/code_generator.hpp"
#include "vm/command_files/compiler_definition.hpp"
#include "vm/command_files/data_block.hpp"

namespace other {

  struct compiled_code_block {
    std::string name;
    lowering_artifact artifact;
  };

  struct compiled_data_section {
    struct field {
      std::string name;
      uint32_t offset;
      uint32_t size;
      /// what it was declared as, or deduced to be; kept for the debug section
      data_type type = OCMD_DATA_TYPE_INVALID;
    };
    std::string name;
    ostd::vector<field> fields;
    ostd::vector<uint8_t> data;
  };

  struct ocmd_program {
    vm_version compiler_version;
    ostd::vector<compiler_definition> definitions;
    ostd::vector<compiled_code_block> compiled_blocks;
    ostd::vector<compiled_data_section> compiled_data_sections;
    /// the asset blocks, as they were read; the linker writes each as its footprint
    ostd::vector<asset_node> assets;
    /// for each asset, where its block starts in the source, or where its declaration does
    ///  when the source was generated
    ostd::vector<source_location> asset_origins;

    bool valid = false;
  };

  struct ocmd_unit {
    std::string name;
    ocmd_program program;
  };

  struct ocmd_link_options {
    bool strict = false;
    /// write the names of blocks, labels and fields after the program. on while the tools are
    ///  young; this default is the one to flip when a build should carry them only on request
    bool debug_info = true;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_FILES_OCMD_PROGRAM_HPP