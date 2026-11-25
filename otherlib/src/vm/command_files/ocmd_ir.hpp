/**
 * \file vm/command_files/ocmd_ir.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_FILES_OCMD_IR_HPP
#define OTHERLIB_VM_COMMAND_FILES_OCMD_IR_HPP

#include "vm/command_files/asset_block.hpp"
#include "vm/command_files/code_block.hpp"
#include "vm/command_files/compiler_definition.hpp"
#include "vm/command_files/data_block.hpp"
#include "vm/diagnostics/vm_diagnostic.hpp"
#include "vm/vm_version.hpp"

namespace other {

  struct ocmd_ir {
    vm_version target_vm_version;
    ostd::vector<compiler_definition> definitions = {};
    ostd::vector<code_block> code_blocks = {};
    ostd::vector<data_block> data_blocks = {};
    ostd::vector<asset_node> assets = {};
    /// for each asset, where its block starts in the source
    ostd::vector<source_location> asset_origins = {};

    bool valid = false;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_FILES_OCMD_IR_HPP