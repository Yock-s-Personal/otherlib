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
#include "vm/command_language/command_lexer.hpp"
#include "vm/command_language/command_lowering.hpp"
#include "vm/command_language/command_parser.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/diagnostics/diagnostic_sink.hpp"
#include "vm/diagnostics/ocmd_trace_sink.hpp"
#include "vm/other_device.hpp"

namespace other {
  namespace {

    /// a diagnostic carries a line and a column but not its file, so the sink adds the name
    class command_error_sink : public diagnostic_sink {
     public:
      explicit command_error_sink(const std::string_view unit)
          : unit(unit) {}
      ~command_error_sink() override = default;

      void handle(diagnostic& diag) override {
        if (diag.severity >= VM_DIAGNOSTIC_ERROR) {
          CORE_LOG_ERROR("[OCMD] {}:{}:{}: {}", unit, diag.span.start.line, diag.span.start.column, diag.final_message);
        } else if (diag.severity == VM_DIAGNOSTIC_WARNING) {
          CORE_LOG_WARN("[OCMD] {}:{}:{}: {}", unit, diag.span.start.line, diag.span.start.column, diag.final_message);
        }
      }

     private:
      std::string unit;
    };

  }  // namespace

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

  ostd::vector<uint8_t> ocmd_toolchain::assemble_command_source(other_command_device* device, const filepath& path) {
    OTHER_ASSERT(device != nullptr && device->bus != nullptr, "Device and its bus must be valid to assemble: {}", path.string());
    OTHER_ASSERT(path.extension() == ".ocmd", "File is not an OCMD source file: {}", path.string());
    PROFILE_SECTION("ocmd_toolchain::assemble_command_source");

    /// each stage has logged its own errors by the time it comes back empty
    diagnostic_engine diagnostics;
    const ostd::vector<command_unit> units = read_command_units(path, path.generic_string(), &diagnostics);
    if (units.empty()) {
      return {};
    }
    const ostd::vector<oasm_source> sources = lower_command_units(*device->bus, units, &diagnostics);
    if (sources.empty()) {
      return {};
    }
    return build_obin(*device->bus, sources, &diagnostics).bytes;
  }

  opt<command_file> ocmd_toolchain::parse_command_source(const std::string_view name, const std::string_view text, diagnostic_engine* diag) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine cannot be null");
    PROFILE_SECTION("ocmd_toolchain::parse_command_source");

    command_error_sink sink{ name };
    const scoped_diagnostic_sink sink_scope(diag, "command-error-sink", &sink);

    command_lexer lexer{ text };
    const ostd::vector<command_token> tokens = lexer.tokenize(diag);
    if (lexer.failed()) {
      return std::nullopt;
    }
    command_file file = command_parser{ tokens }.parse(diag);
    if (!file.valid) {
      return std::nullopt;
    }
    return file;
  }

  ostd::vector<command_unit> ocmd_toolchain::read_command_units(const filepath& root, const std::string_view root_name, diagnostic_engine* diag) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine cannot be null");
    PROFILE_SECTION("ocmd_toolchain::read_command_units");

    struct pending_file {
      filepath path;
      std::string name;
    };
    ostd::vector<pending_file> pending = { { .path = root, .name = std::string{ root_name } } };
    ostd::vector<filepath> seen;
    ostd::vector<command_unit> units;

    /// pending grows while it is walked: each file adds the files it uses
    for (size_t next = 0; next < pending.size(); ++next) {
      const pending_file current = pending[next];
      std::error_code ec;
      const filepath canonical = std::filesystem::weakly_canonical(current.path, ec);
      if (std::ranges::find(seen, canonical) != seen.end()) {
        continue;
      }
      seen.push_back(canonical);

      std::ifstream file(current.path, std::ios::binary);
      if (!file.is_open()) {
        CORE_LOG_ERROR("[VM] : '{}' does not exist or cannot be read", current.name);
        return {};
      }
      std::ostringstream text;
      text << file.rdbuf();

      opt<command_file> parsed = parse_command_source(current.name, text.str(), diag);
      if (!parsed.has_value()) {
        CORE_LOG_ERROR("[VM] : '{}' has errors and was not built", current.name);
        return {};
      }

      /// a 'use' path is relative to the file that names it
      for (const command_use& use : parsed->uses) {
        pending.push_back({ .path = current.path.parent_path() / use.path, .name = (filepath{ current.name }.parent_path() / use.path).generic_string() });
      }
      units.push_back({ .name = current.name, .file = std::move(*parsed) });
    }
    return units;
  }

  ostd::vector<oasm_source> ocmd_toolchain::lower_command_units(const command_bus& bus, std::span<const command_unit> units, diagnostic_engine* diag, ocmd_link_options options) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine cannot be null");
    PROFILE_SECTION("ocmd_toolchain::lower_command_units");

    ostd::vector<oasm_source> sources;
    sources.reserve(units.size());
    bool success = true;
    for (size_t i = 0; i < units.size(); ++i) {
      /// generated names carry the file's stem, so two files with one stem would define the same symbols
      const std::string stem = command_lowering::unit_stem(units[i].name);
      if (std::ranges::any_of(units.first(i), [&stem](const command_unit& earlier) { return command_lowering::unit_stem(earlier.name) == stem; })) {
        CORE_LOG_ERROR("[VM] : two files in this build are named '{}'; each .ocmd file in a build needs its own name", stem);
        success = false;
        continue;
      }

      /// every function in the build can be called from every file; 'use' decides what is in the build
      ostd::vector<command_function_signature> imported;
      for (size_t j = 0; j < units.size(); ++j) {
        if (j == i) {
          continue;
        }
        for (const command_function& function : units[j].file.functions) {
          imported.push_back({ .name = function.name, .param_count = function.params.size() });
        }
      }

      command_error_sink sink{ units[i].name };
      const scoped_diagnostic_sink sink_scope(diag, "command-error-sink", &sink);
      command_lowering_result lowered = command_lowering{ bus, units[i].name, imported, { .entry_unit = i == 0, .strict = options.strict } }.lower(units[i].file, diag);
      if (!lowered.success) {
        CORE_LOG_ERROR("[VM] : '{}' has errors and was not built", units[i].name);
        success = false;
        continue;
      }
      sources.push_back({ .name = units[i].name, .text = std::move(lowered.oasm) });
    }

    if (!success) {
      sources.clear();
    }
    return sources;
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