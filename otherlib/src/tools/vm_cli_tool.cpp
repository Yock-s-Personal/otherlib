/**
 * \file tools/vm_cli_tool.cpp
 **/
#include "tools/vm_cli_tool.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/profiler.hpp"

#include "tools/scene_cli_tool.hpp"
#include "vm/builtin_command_devices.hpp"
#include "vm/command_bus.hpp"
#include "vm/command_files/ocmd_headers.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/vm.hpp"

namespace other {
  namespace cli {
    namespace {

      constexpr std::string_view kUsage =
        "usage: oecli vm <command> <input...> [options]\n"
        "\n"
        "commands:\n"
        "  build <a.ocmd|a.oasm> [...] -o <out.obin>   compile each source and link them into one program\n"
        "  info <program.obin>                         print a linked program's header\n"
        "\n"
        "the first .ocmd input is the entry file; the files it uses are found and built with it\n"
        "\n"
        "options:\n"
        "  -o, --out <path>   output path (build only)\n"
        "      --strict       treat warnings as errors and write nothing (build only)\n"
        "      --emit-oasm    also write the OASM made from each .ocmd file, beside the output (build only)";

      struct parsed_arguments {
        std::string command = "";
        ostd::vector<filepath> inputs = {};
        opt<filepath> output = std::nullopt;
        bool strict = false;
        bool emit_oasm = false;
        opt<std::string> error = std::nullopt;
      };

      /// the scene tool's loop, taking any number of inputs
      parsed_arguments parse_arguments(std::span<const std::string> args) {
        parsed_arguments parsed = {};
        if (args.empty()) {
          parsed.error = "missing command";
          return parsed;
        }
        parsed.command = args[0];

        for (size_t i = 1; i < args.size(); ++i) {
          const std::string& arg = args[i];
          if (arg == "-o" || arg == "--out") {
            if (i + 1 >= args.size()) {
              parsed.error = std::format("'{}' expects a path argument", arg);
              return parsed;
            }
            parsed.output = filepath(args[++i]);
          } else if (arg == "--strict") {
            parsed.strict = true;
          } else if (arg == "--emit-oasm") {
            parsed.emit_oasm = true;
          } else if (!arg.empty() && arg.front() == '-') {
            parsed.error = std::format("unknown option '{}'", arg);
            return parsed;
          } else {
            parsed.inputs.push_back(filepath(arg));
          }
        }

        if (parsed.inputs.empty()) {
          parsed.error = "missing input file";
        }
        return parsed;
      }

      filepath from_working_directory(const tool_context& ctx, const filepath& path) {
        return path.is_absolute() ? path : ctx.working_directory / path;
      }

      tool_result build(tool_context& ctx, std::span<const filepath> inputs, const filepath& output, const ocmd_link_options options, const bool emit_oasm) {
        PROFILE_SECTION("vm_tool::build");
        diagnostic_engine diagnostics;
        ocmd_toolchain toolchain;

        ostd::vector<oasm_source> sources;
        ostd::vector<command_unit> units;
        sources.reserve(inputs.size());
        for (const filepath& input : inputs) {
          const std::string extension = input.extension().string();
          if (extension == ".ocmd") {
            ostd::vector<command_unit> read = toolchain.read_command_units(from_working_directory(ctx, input), input.generic_string(), &diagnostics);
            if (read.empty()) {
              return tool_result::error(std::format("could not read '{}'; the errors are listed above", input.string()));
            }
            /// two inputs may use the same file
            for (command_unit& unit : read) {
              if (std::ranges::find(units, unit.name, &command_unit::name) == units.end()) {
                units.push_back(std::move(unit));
              }
            }
            continue;
          }
          if (extension != ".oasm") {
            return tool_result::error(std::format("'build' expects .ocmd or .oasm inputs, got '{}'\n{}", input.string(), kUsage));
          }

          std::ifstream file(from_working_directory(ctx, input));
          if (!file.is_open()) {
            return tool_result::error(std::format("input file '{}' does not exist or cannot be read", input.string()));
          }
          std::ostringstream text;
          text << file.rdbuf();

          /// named as typed, so a relative path gives the same link order on every machine
          sources.push_back({ .name = input.generic_string(), .text = text.str() });
        }

        /// inside a running environment the live bus also holds the devices its plugins added
        command_bus local_bus;
        const command_bus* bus = &local_bus;
        if (const other_command_device* live = vm::get_initialized_device(); live != nullptr && live->bus != nullptr) {
          bus = live->bus;
        } else {
          register_builtin_command_devices(local_bus);
        }

        const filepath destination = from_working_directory(ctx, output);
        if (!units.empty()) {
          ostd::vector<oasm_source> lowered = toolchain.lower_command_units(*bus, units, &diagnostics, options);
          if (lowered.empty()) {
            return tool_result::error("build failed; the errors are listed above");
          }
          /// named after the script with .oasm added, so it cannot land on a hand-written .oasm
          for (size_t i = 0; emit_oasm && i < lowered.size(); ++i) {
            const filepath listing = destination.parent_path() / std::format("{}.oasm", filepath{ lowered[i].name }.filename().string());
            std::ofstream out(listing, std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
              return tool_result::error(std::format("failed to write '{}'", listing.string()));
            }
            out << lowered[i].text;
            ctx.print("wrote '{}'", listing.string());
          }
          sources.append_range(std::move(lowered));
        }

        const obin_build_result result = toolchain.build_obin(*bus, sources, &diagnostics, options);
        if (!result.success) {
          return tool_result::error("build failed; the errors are listed above");
        }

        if (!toolchain.write_obin(destination, result.bytes)) {
          return tool_result::error(std::format("failed to write '{}'", destination.string()));
        }
        return tool_result::ok(std::format("linked {} source(s) into '{}' ({} bytes)", sources.size(), destination.string(), result.bytes.size()));
      }

      tool_result info(tool_context& ctx, const filepath& input) {
        PROFILE_SECTION("vm_tool::info");
        std::ifstream file(input, std::ios::binary);
        if (!file.is_open()) {
          return tool_result::error(std::format("input file '{}' does not exist or cannot be read", input.string()));
        }

        const ostd::vector<uint8_t> bytes(std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{});
        if (bytes.size() < sizeof(ocmd_file_header)) {
          return tool_result::error(std::format("'{}' is {} bytes, smaller than a program header", input.string(), bytes.size()));
        }

        ocmd_file_header header;
        std::memcpy(&header, bytes.data(), sizeof(header));
        const size_t code_size = header.prog_header.data_section_offset;
        const size_t program_size = bytes.size() - sizeof(ocmd_file_header);
        const size_t entry_point = header.prog_header.entry_point_address;

        ctx.print("program '{}'", input.filename().string());
        ctx.print("  format version: {}.{}.{}", static_cast<uint32_t>(header.file_version_major), static_cast<uint32_t>(header.file_version_minor),
                  static_cast<uint32_t>(header.file_version_patch));
        ctx.print("  linked by engine: {}.{}.{}", static_cast<uint32_t>(header.environment_version_major), static_cast<uint32_t>(header.environment_version_minor),
                  static_cast<uint32_t>(header.environment_version_patch));
        ctx.print("  size: {} bytes ({} code, {} data)", program_size, std::min(code_size, program_size), program_size - std::min(code_size, program_size));
        ctx.print("  entry point: {:#06x}", entry_point);

        if (const opt<std::string> refusal = ocmd_toolchain{}.check_obin(bytes); refusal.has_value()) {
          return tool_result::error(std::format("this build will not load it: {}", *refusal));
        }
        return tool_result::ok("this build can load it");
      }

    }  // namespace

    std::string_view vm_tool::usage() const {
      return kUsage;
    }

    tool_result vm_tool::execute(tool_context& ctx, std::span<const std::string> args) {
      PROFILE_SECTION("vm_tool::execute");
      ensure_cli_runtime();

      const parsed_arguments parsed = parse_arguments(args);
      if (parsed.error.has_value()) {
        return tool_result::error(std::format("{}\n{}", *parsed.error, kUsage));
      }

      if (parsed.command == "build") {
        if (!parsed.output.has_value()) {
          return tool_result::error(std::format("'build' needs -o/--out\n{}", kUsage));
        }
        return build(ctx, parsed.inputs, *parsed.output, { .strict = parsed.strict }, parsed.emit_oasm);
      }
      if (parsed.command == "info") {
        if (parsed.inputs.size() != 1 || parsed.output.has_value() || parsed.strict) {
          return tool_result::error(std::format("'info' takes one input and no options\n{}", kUsage));
        }
        return info(ctx, from_working_directory(ctx, parsed.inputs.front()));
      }
      return tool_result::error(std::format("unknown command '{}'\n{}", parsed.command, kUsage));
    }

  }  // namespace cli
}  // namespace other
