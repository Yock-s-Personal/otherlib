/**
 * \file vm/command_files/ocmd_toolchain.cpp
 **/
#include "vm/command_files/ocmd_toolchain.hpp"

#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "vm/code_generator_000.hpp"
#include "vm/command_bus.hpp"
#include "vm/command_files/lexer.hpp"
#include "vm/command_files/oasm_parser.hpp"
#include "vm/command_files/ocmd_compiler.hpp"
#include "vm/command_files/ocmd_linker.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/diagnostics/ocmd_trace_sink.hpp"
#include "vm/other_device.hpp"

namespace other {

  ostd::vector<uint8_t> ocmd_toolchain::assemble_oasm_source(other_command_device* device, const filepath& path) {
    OTHER_ASSERT(device != nullptr && device->bus != nullptr, "Device and its bus must be valid to assemble: {}", path.string());
    OTHER_ASSERT(std::filesystem::exists(path), "File does not exist: {}", path.string());
    OTHER_ASSERT(path.extension() == ".oasm", "File is not an OASM source file: {}", path.string());
    PROFILE_SECTION("ocmd_toolchain::assemble_oasm_source");

    oasm_source source = { .name = path.generic_string() };
    {
      PROFILE_SECTION("ocmd_toolchain::assemble_oasm_source--read_source");
      std::ifstream file(path.string());
      OTHER_ASSERT(file, "Failed to open file: {}", path.string());

      std::ostringstream ss;
      ss << file.rdbuf();
      source.text = ss.str();
    }

    if (source.text.empty()) {
      return {};
    }

    diagnostic_engine diagnostics;
    ocmd_trace_sink trace_sink;
    natural_t trace_id = diagnostics.register_sink("trace-sink", &trace_sink);

    obin_build_result result = build_obin(*device->bus, std::span{ &source, 1 }, &diagnostics);

    diagnostics.remove_sink(trace_id);
    return result.bytes;
  }

  obin_build_result ocmd_toolchain::build_obin(const command_bus& bus, std::span<const oasm_source> sources, diagnostic_engine* diag, ocmd_link_options options) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine cannot be null");
    PROFILE_SECTION("ocmd_toolchain::build_obin");

    obin_build_result result;
    if (sources.empty()) {
      CORE_LOG_ERROR("[VM] : nothing to build, no sources were given");
      return result;
    }

    ostd::vector<ocmd_unit> units;
    units.reserve(sources.size());
    for (const oasm_source& source : sources) {
      /// stops at the first file that fails; its errors have already gone to diag
      try {
        units.push_back({ .name = source.name, .program = ocmd_compiler{}.compile(source.text, make_scope<code_generator_000>(), diag) });
      } catch (const std::exception& e) {
        CORE_LOG_ERROR("[VM] : failed to compile '{}': {}", source.name, e.what());
        return result;
      } catch (...) {
        CORE_LOG_ERROR("[VM] : failed to compile '{}': unknown exception", source.name);
        return result;
      }
    }

    try {
      result.bytes = ocmd_linker{ std::span<const ocmd_unit>{ units }, options }.link(bus.create_default_symbol_resolver(), diag);
      result.success = !result.bytes.empty();
    } catch (const std::exception& e) {
      CORE_LOG_ERROR("[VM] : {}", e.what());
      result.bytes = {};
    } catch (...) {
      CORE_LOG_ERROR("[VM] : unknown exception caught while linking");
      result.bytes = {};
    }
    return result;
  }

  opt<std::string> ocmd_toolchain::check_obin(std::span<const uint8_t> bytes) const {
    if (bytes.size() < sizeof(ocmd_file_header)) {
      return std::format("{} bytes is smaller than the {}-byte header", bytes.size(), sizeof(ocmd_file_header));
    }

    ocmd_file_header header;
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (std::string_view{ header.file_signature, sizeof(header.file_signature) } != "OCMD") {
      return "the file signature is not 'OCMD'";
    }

    /// below 1.0 a minor bump is a breaking change
    if (header.file_version_major != OCMD_FILE_FORMAT_VERSION_MAJOR || header.file_version_minor != OCMD_FILE_FORMAT_VERSION_MINOR) {
      return std::format("it has format version {}.{}.{} and this build reads {}",
                         static_cast<uint32_t>(header.file_version_major), static_cast<uint32_t>(header.file_version_minor),
                         static_cast<uint32_t>(header.file_version_patch), OCMD_FILE_FORMAT_VERSION_STRING);
    }

    /// its syscalls hold the device and function ids of the engine that linked it
    if (header.environment_version_major != OTHER_ENVIRONMENT_VERSION_MAJOR || header.environment_version_minor != OTHER_ENVIRONMENT_VERSION_MINOR ||
        header.environment_version_patch != OTHER_ENVIRONMENT_VERSION_PATCH) {
      return std::format("it was linked by engine {}.{}.{} and this is {}; build it again",
                         static_cast<uint32_t>(header.environment_version_major), static_cast<uint32_t>(header.environment_version_minor),
                         static_cast<uint32_t>(header.environment_version_patch), OTHER_ENVIRONMENT_VERSION_STRING);
    }

    const size_t program_size = bytes.size() - sizeof(ocmd_file_header);
    const size_t stated_size = header.prog_header.program_size;
    const size_t code_start = header.prog_header.code_section_offset;
    const size_t data_start = header.prog_header.data_section_offset;
    const size_t entry_point = header.prog_header.entry_point_address;
    if (stated_size != program_size) {
      return std::format("its header counts {} program bytes and the file holds {}", stated_size, program_size);
    }
    if (program_size > other_command_device::kMaxProgramSize) {
      return std::format("its {} program bytes are more than the {} the device holds", program_size, other_command_device::kMaxProgramSize);
    }
    if (code_start > data_start || data_start > program_size) {
      return "its section offsets do not fit the file";
    }
    if (entry_point < code_start || entry_point >= data_start) {
      return "its entry point is outside the code section";
    }
    return std::nullopt;
  }

  bool ocmd_toolchain::write_obin(const filepath& path, std::span<const uint8_t> bytes) {
    PROFILE_SECTION("ocmd_toolchain::write_obin");
    if (const opt<std::string> refusal = check_obin(bytes); refusal.has_value()) {
      CORE_LOG_ERROR("[VM] : not writing '{}': {}", path.string(), *refusal);
      return false;
    }

    if (path.has_parent_path()) {
      std::error_code ec;
      std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      CORE_LOG_ERROR("[VM] : failed to open '{}' for writing", path.string());
      return false;
    }

    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return file.good();
  }

  ostd::vector<uint8_t> ocmd_toolchain::read_obin(const filepath& path) {
    PROFILE_SECTION("ocmd_toolchain::read_obin");
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
      CORE_LOG_ERROR("[VM] : failed to open '{}' for reading", path.string());
      return {};
    }

    ostd::vector<uint8_t> bytes(std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{});
    if (const opt<std::string> refusal = check_obin(bytes); refusal.has_value()) {
      CORE_LOG_ERROR("[VM] : refusing '{}': {}", path.string(), *refusal);
      return {};
    }
    return bytes;
  }

}  // namespace other