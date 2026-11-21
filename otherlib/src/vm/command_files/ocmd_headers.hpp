/**
 * \file vm/command_files/ocmd_headers.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_FILES_OCMD_HEADERS_HPP
#define OTHERLIB_VM_COMMAND_FILES_OCMD_HEADERS_HPP

#include <cstdint>

#include "core/version.hpp"

#include "vm/ocmd_file_format_version.hpp"

namespace other {

#pragma pack(push, 1)
  struct program_header {
    uint16_t code_section_offset = 0;  // == size of data table
    uint16_t data_section_offset = 0;  // == size of data table + size of code section
    uint16_t entry_point_address = 0;  // must be inside code section
    uint16_t program_size = 0;         // bytes that follow the file header
    uint8_t has_code_flag = 0;
    uint8_t has_debug_flag = 0;  // a debug section follows the program bytes
    uint8_t reserved[6] = {};
  };
  static_assert(sizeof(program_header) == 16, "Program header size must be 16 bytes");

  struct ocmd_file_header {
    char file_signature[4] = { 'O', 'C', 'M', 'D' };
    uint8_t file_version_major = OCMD_FILE_FORMAT_VERSION_MAJOR;
    uint8_t file_version_minor = OCMD_FILE_FORMAT_VERSION_MINOR;
    uint8_t file_version_patch = OCMD_FILE_FORMAT_VERSION_PATCH;
    uint8_t environment_version_major = OTHER_ENVIRONMENT_VERSION_MAJOR;
    uint8_t environment_version_minor = OTHER_ENVIRONMENT_VERSION_MINOR;
    uint8_t environment_version_patch = OTHER_ENVIRONMENT_VERSION_PATCH;
    uint8_t reserved[6] = { 0 };
    program_header prog_header = {};
  };
#pragma pack(pop)
  static_assert(sizeof(ocmd_file_header) == 32, "OCMD file header size must be 32 bytes");

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_FILES_OCMD_HEADERS_HPP