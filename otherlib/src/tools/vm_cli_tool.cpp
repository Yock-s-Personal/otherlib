/**
 * \file tools/vm_cli_tool.cpp
 **/
#include "tools/vm_cli_tool.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/profiler.hpp"

#include "project/project.hpp"
#include "tools/scene_cli_tool.hpp"
#include "vm/builtin_command_devices.hpp"
#include "vm/command_bus.hpp"
#include "vm/command_files/asset_footprint.hpp"
#include "vm/command_files/ocmd_debug_info.hpp"
#include "vm/command_files/ocmd_headers.hpp"
#include "vm/command_files/ocmd_toolchain.hpp"
#include "vm/decompiler.hpp"
#include "vm/diagnostics/diagnostic_engine.hpp"
#include "vm/project_program.hpp"
#include "vm/vm.hpp"

namespace other {
  namespace cli {
    namespace {

      constexpr std::string_view kUsage =
        "usage: oecli vm <command> <input...> [options]\n"
        "\n"
        "commands:\n"
        "  build <source> [...] -o <out.obin>          compile each source and link them into one program\n"
        "  build --project <project> -o <out.obin>     link a project's boot program: its VM sources behind a generated entry\n"
        "  view <program>                              print a linked program's instructions, data and assets\n"
        "  view <source> [...]                         build the sources in memory and print the result\n"
        "  data <program>                              print each data field: its type, its value, where it is used\n"
        "  data <source> [...]                         build the sources in memory and print their data\n"
        "  info <program.obin>                         print a linked program's header and what follows the program\n"
        "  check <file> [...]                          read the files and what they use, and hold every declaration to its\n"
        "                                              struct; builds nothing. a file not named .ocmd is a file of declarations\n"
        "\n"
        "a source is OASM when it is named .oasm, a script when it is named .ocmd, and otherwise a file of\n"
        "declarations. the first that is not .oasm is the entry file; the files it uses are found and built with it\n"
        "\n"
        "options:\n"
        "  -o, --out <path>   output path (build only)\n"
        "      --project <p>  a project file or its folder, in place of sources (build only). the program loads the\n"
        "                     project by this path, so it is run from the folder it was built in\n"
        "      --strict       treat warnings as errors and write nothing (build and view)\n"
        "      --no-debug     leave the debug section out of the program (build), or show it without names (view, data).\n"
        "                     the assets are in the program either way; without names their kinds and fields are hashes\n"
        "      --emit-oasm    also write the OASM made from each script or file of declarations, beside the\n"
        "                     output (build only);\n"
        "                     with --project, the generated entry";

      struct parsed_arguments {
        std::string command = "";
        ostd::vector<filepath> inputs = {};
        opt<filepath> output = std::nullopt;
        opt<filepath> project = std::nullopt;
        bool strict = false;
        bool emit_oasm = false;
        bool no_debug = false;
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
          } else if (arg == "--project") {
            if (i + 1 >= args.size()) {
              parsed.error = std::format("'{}' expects a path argument", arg);
              return parsed;
            }
            parsed.project = filepath(args[++i]);
          } else if (arg == "--strict") {
            parsed.strict = true;
          } else if (arg == "--emit-oasm") {
            parsed.emit_oasm = true;
          } else if (arg == "--no-debug") {
            parsed.no_debug = true;
          } else if (!arg.empty() && arg.front() == '-') {
            parsed.error = std::format("unknown option '{}'", arg);
            return parsed;
          } else {
            parsed.inputs.push_back(filepath(arg));
          }
        }

        if (parsed.project.has_value() && (parsed.command != "build" || !parsed.inputs.empty())) {
          parsed.error = "'--project' goes with 'build' and takes the place of its sources";
        } else if (parsed.inputs.empty() && !parsed.project.has_value()) {
          parsed.error = "missing input file";
        }
        return parsed;
      }

      filepath from_working_directory(const tool_context& ctx, const filepath& path) {
        return path.is_absolute() ? path : ctx.working_directory / path;
      }

      struct linked_program {
        ostd::vector<uint8_t> bytes;
        size_t source_count = 0;
        /// how many assets the sources hold between them; the program's asset section has them
        size_t asset_count = 0;
        /// set when an input could not be read or the program did not build
        opt<std::string> error = std::nullopt;
      };

      /// inside a running environment the live bus also holds the devices its plugins added
      const command_bus& tool_bus(command_bus& local_bus) {
        if (const other_command_device* live = vm::get_initialized_device(); live != nullptr && live->bus != nullptr) {
          return *live->bus;
        }
        register_builtin_command_devices(local_bus);
        return local_bus;
      }

      /// reads the sources, lowers the .ocmd ones and links everything in memory. listing_folder,
      ///  when set, is where the OASM made from each .ocmd file is written
      linked_program link_sources(tool_context& ctx, std::span<const filepath> inputs, const command_bus& bus, const ocmd_link_options options, const opt<filepath>& listing_folder) {
        PROFILE_SECTION("vm_tool::link_sources");
        diagnostic_engine diagnostics;
        ocmd_toolchain toolchain;

        ostd::vector<oasm_source> sources;
        ostd::vector<command_unit> units;
        sources.reserve(inputs.size());
        for (const filepath& input : inputs) {
          /// anything that is not OASM is of the command language: a script, or a file of declarations
          if (input.extension() != ".oasm") {
            ostd::vector<command_unit> read = toolchain.read_command_units(from_working_directory(ctx, input), input.generic_string(), &diagnostics);
            if (read.empty()) {
              return { .error = std::format("could not read '{}'; the errors are listed above", input.string()) };
            }
            /// two inputs may use the same file
            for (command_unit& unit : read) {
              if (std::ranges::find(units, unit.name, &command_unit::name) == units.end()) {
                units.push_back(std::move(unit));
              }
            }
            continue;
          }

          std::ifstream file(from_working_directory(ctx, input));
          if (!file.is_open()) {
            return { .error = std::format("input file '{}' does not exist or cannot be read", input.string()) };
          }
          std::ostringstream text;
          text << file.rdbuf();

          /// named as typed, so a relative path gives the same link order on every machine
          sources.push_back({ .name = input.generic_string(), .text = text.str() });
        }

        if (!units.empty()) {
          ostd::vector<oasm_source> lowered = toolchain.lower_command_units(bus, units, &diagnostics, options);
          if (lowered.empty()) {
            return { .error = "build failed; the errors are listed above" };
          }
          /// the listings are written before the program, and it is the program's write that
          ///  makes the output folder
          if (listing_folder.has_value()) {
            std::error_code ec;
            std::filesystem::create_directories(*listing_folder, ec);
          }
          /// named after the script with .oasm added, so it cannot land on a hand-written .oasm
          for (size_t i = 0; listing_folder.has_value() && i < lowered.size(); ++i) {
            const filepath listing = *listing_folder / std::format("{}.oasm", filepath{ lowered[i].name }.filename().string());
            std::ofstream out(listing, std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
              return { .error = std::format("failed to write '{}'", listing.string()) };
            }
            out << lowered[i].text;
            ctx.print("wrote '{}'", listing.string());
          }
          sources.append_range(std::move(lowered));
        }

        const ostd::vector<ocmd_unit> compiled = toolchain.compile_units(sources, &diagnostics);
        if (compiled.empty()) {
          return { .error = "build failed; the errors are listed above" };
        }
        obin_build_result result = toolchain.link_units(bus, compiled, &diagnostics, options);
        if (!result.success) {
          return { .error = "build failed; the errors are listed above" };
        }

        linked_program linked = { .bytes = std::move(result.bytes), .source_count = sources.size() };
        for (const ocmd_unit& unit : compiled) {
          linked.asset_count += unit.program.assets.size();
        }
        return linked;
      }

      /// one print per line, as the consoles this runs in expect; the text ends in a line break
      void print_lines(tool_context& ctx, std::string_view text) {
        while (!text.empty()) {
          const size_t line_end = text.find('\n');
          ctx.print("{}", text.substr(0, line_end));
          text.remove_prefix(line_end == std::string_view::npos ? text.size() : line_end + 1);
        }
      }

      tool_result build(tool_context& ctx, std::span<const filepath> inputs, const filepath& output, const ocmd_link_options options, const bool emit_oasm) {
        PROFILE_SECTION("vm_tool::build");
        command_bus local_bus;
        const command_bus& bus = tool_bus(local_bus);

        const filepath destination = from_working_directory(ctx, output);
        const linked_program linked = link_sources(ctx, inputs, bus, options, emit_oasm ? opt<filepath>{ destination.parent_path() } : std::nullopt);
        if (linked.error.has_value()) {
          return tool_result::error(*linked.error);
        }

        if (!ocmd_toolchain{}.write_obin(destination, linked.bytes)) {
          return tool_result::error(std::format("failed to write '{}'", destination.string()));
        }
        if (linked.asset_count > 0) {
          ctx.print("{} asset(s) written to the program's asset section, {} bytes that are not loaded into the device", linked.asset_count, obin_asset_bytes(linked.bytes).size());
        }
        return tool_result::ok(std::format("linked {} source(s) into '{}' ({} bytes)", linked.source_count, destination.string(), linked.bytes.size()));
      }

      /// a project's boot program: every VM source it has, behind the entry the engine writes
      tool_result build_project(tool_context& ctx, const filepath& project_path, const filepath& output, const ocmd_link_options options, const bool emit_oasm) {
        PROFILE_SECTION("vm_tool::build_project");
        command_bus local_bus;
        const command_bus& bus = tool_bus(local_bus);

        const opt<filepath> project_file = project::find_project_file(from_working_directory(ctx, project_path));
        if (!project_file.has_value()) {
          return tool_result::error(std::format("'{}' is not a project file, nor a folder holding a .toml or .oproj file of its own name", project_path.string()));
        }
        /// the program loads the project by the path typed here, so it runs from this folder on any machine
        const filepath as_typed = *project_file == from_working_directory(ctx, project_path) ? project_path : project_path / project_file->filename();

        diagnostic_engine diagnostics;
        const project_program_result built = build_project_program(bus, *project_file, &diagnostics, { .project_load_path = as_typed.generic_string(), .link = options });
        for (const project_hook_handler& handler : built.hooks) {
          ctx.print("hook {}: '{}' in '{}'", hook_name(handler.hook), handler.block, handler.unit);
        }
        for (const std::string& handler : built.uncalled_handlers) {
          ctx.print("handler for {}: linked in, and not called by anything yet", handler);
        }
        if (!built.success) {
          return tool_result::error("build failed; the errors are listed above");
        }

        /// the program first: writing it makes the output folder, which the listing needs
        const filepath destination = from_working_directory(ctx, output);
        if (!ocmd_toolchain{}.write_obin(destination, built.bytes)) {
          return tool_result::error(std::format("failed to write '{}'", destination.string()));
        }
        if (emit_oasm) {
          const filepath listing = destination.parent_path() / "__boot.oasm";
          std::ofstream out(listing, std::ios::binary | std::ios::trunc);
          if (!out.is_open()) {
            return tool_result::error(std::format("failed to write '{}'", listing.string()));
          }
          out << built.boot_unit;
          ctx.print("wrote '{}'", listing.string());
        }
        return tool_result::ok(std::format("linked {} project source(s) and the generated entry into '{}' ({} bytes)", built.sources.size(), destination.string(), built.bytes.size()));
      }

      /// a program as text: 'view' is its instructions and data, 'data' is its data alone with
      ///  where each field is used. given sources instead of a program, it builds them in
      ///  memory first, so nothing is written
      tool_result view(tool_context& ctx, const std::string_view command, std::span<const filepath> inputs, const ocmd_link_options options) {
        PROFILE_SECTION("vm_tool::view");
        command_bus local_bus;
        const command_bus& bus = tool_bus(local_bus);

        /// a linked program says what it is in its first four bytes, whatever it is called:
        ///  'build -o' takes any name. a file that does not is a source
        const auto is_source = [&ctx](const filepath& input) -> bool {
          if (input.extension() == ".ocmd" || input.extension() == ".oasm") {
            return true;
          }
          std::ifstream file(from_working_directory(ctx, input), std::ios::binary);
          char signature[4] = {};
          file.read(signature, sizeof(signature));
          return file.is_open() && std::string_view{ signature, static_cast<size_t>(file.gcount()) } != "OCMD";
        };
        ostd::vector<uint8_t> bytes;
        if (!std::ranges::all_of(inputs, is_source)) {
          if (inputs.size() != 1) {
            return tool_result::error(std::format("'{}' takes one program, or the sources of one program\n{}", command, kUsage));
          }
          std::ifstream file(from_working_directory(ctx, inputs.front()), std::ios::binary);
          if (!file.is_open()) {
            return tool_result::error(std::format("input file '{}' does not exist or cannot be read", inputs.front().string()));
          }
          bytes.assign(std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{});
          /// asked to show it without its names: drop the section and the flag that announces
          ///  it. the assets, which come after it, stay
          if (!options.debug_info && !obin_debug_bytes(bytes).empty()) {
            ocmd_file_header header;
            std::memcpy(&header, bytes.data(), sizeof(header));
            header.prog_header.has_debug_flag = 0;
            const std::span<const uint8_t> program = obin_program_bytes(bytes);
            const std::span<const uint8_t> assets = obin_asset_bytes(bytes);
            ostd::vector<uint8_t> without(sizeof(header));
            std::memcpy(without.data(), &header, sizeof(header));
            without.insert(without.end(), program.begin(), program.end());
            without.insert(without.end(), assets.begin(), assets.end());
            bytes = std::move(without);
          }
        } else {
          linked_program linked = link_sources(ctx, inputs, bus, options, std::nullopt);
          if (linked.error.has_value()) {
            return tool_result::error(*linked.error);
          }
          bytes = std::move(linked.bytes);
        }

        /// the names in a debug section are the paths the sources were given by, so they are
        ///  looked for from the working directory. a file that is not there is simply not shown
        ostd::unordered_map<std::string, ostd::vector<std::string>> files;
        const source_line_reader read_line = [&ctx, &files](const std::string_view unit, const uint32_t line) -> opt<std::string> {
          const auto [entry, is_new] = files.try_emplace(std::string{ unit });
          if (is_new) {
            std::ifstream file(from_working_directory(ctx, filepath{ unit }));
            for (std::string text; std::getline(file, text);) {
              if (!text.empty() && text.back() == '\r') {
                text.pop_back();
              }
              entry->second.push_back(text);
            }
          }
          if (line == 0 || line > entry->second.size()) {
            return std::nullopt;
          }
          const std::string& text = entry->second[line - 1];
          const size_t first = text.find_first_not_of(" \t");
          return first == std::string::npos ? std::string{} : text.substr(first);
        };

        const disassembly listing = command == "data" ? decompiler::list_data(bytes, read_line) : decompiler::disassemble(bytes, &bus, read_line);
        if (!listing.error.empty()) {
          return tool_result::error(std::format("'{}' cannot be shown: {}", inputs.front().string(), listing.error));
        }
        print_lines(ctx, listing.text);
        return tool_result::ok();
      }

      /// reads what a build would read and holds every declaration to its struct. makes nothing
      tool_result check(tool_context& ctx, std::span<const filepath> inputs) {
        PROFILE_SECTION("vm_tool::check");
        diagnostic_engine diagnostics;
        ocmd_toolchain toolchain;

        ostd::vector<command_unit> units;
        for (const filepath& input : inputs) {
          ostd::vector<command_unit> read = toolchain.read_command_units(from_working_directory(ctx, input), input.generic_string(), &diagnostics);
          if (read.empty()) {
            return tool_result::error(std::format("could not read '{}'; the errors are listed above", input.string()));
          }
          /// two inputs may use the same file
          for (command_unit& unit : read) {
            const auto known = std::ranges::find(units, unit.name, &command_unit::name);
            if (known == units.end()) {
              units.push_back(std::move(unit));
            } else {
              known->included |= unit.included;
            }
          }
        }

        command_check_counts counts;
        if (!toolchain.check_command_units(units, &diagnostics, &counts)) {
          return tool_result::error("check failed; the errors are listed above");
        }
        for (const command_unit& unit : units) {
          ctx.print("read '{}'", unit.name);
        }
        return tool_result::ok(std::format("checked {} file(s): {} enum(s), {} struct(s), {} declaration(s), and nothing wrong", units.size(), counts.enums, counts.structs, counts.declarations));
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
        const size_t program_size = obin_program_bytes(bytes).size();
        const size_t debug_size = obin_debug_bytes(bytes).size();
        const size_t entry_point = header.prog_header.entry_point_address;

        ctx.print("program '{}'", input.filename().string());
        ctx.print("  format version: {}.{}.{}", static_cast<uint32_t>(header.file_version_major), static_cast<uint32_t>(header.file_version_minor),
                  static_cast<uint32_t>(header.file_version_patch));
        ctx.print("  linked by engine: {}.{}.{}", static_cast<uint32_t>(header.environment_version_major), static_cast<uint32_t>(header.environment_version_minor),
                  static_cast<uint32_t>(header.environment_version_patch));
        ctx.print("  size: {} bytes ({} code, {} data)", program_size, std::min(code_size, program_size), program_size - std::min(code_size, program_size));
        ctx.print("  entry point: {:#06x}", entry_point);
        if (debug_size > 0) {
          ctx.print("  debug section: {} bytes, not loaded into the device", debug_size);
        } else {
          ctx.print("  debug section: none");
        }
        const std::span<const uint8_t> asset_bytes = obin_asset_bytes(bytes);
        if (const opt<obin_assets> assets = obin_assets::decode(asset_bytes); assets.has_value()) {
          ctx.print("  asset section: {} bytes, {} asset(s), not loaded into the device", asset_bytes.size(), assets->assets.size());
        } else if (asset_bytes.empty()) {
          ctx.print("  asset section: none");
        } else {
          ctx.print("  asset section: {} bytes that cannot be read", asset_bytes.size());
        }

        if (const opt<std::string> refusal = ocmd_toolchain{}.check_obin(bytes); refusal.has_value()) {
          return tool_result::error(std::format("this build will not load it: {}", *refusal));
        }
        return tool_result::ok("this build can load it");
      }

    }  // namespace

    const command_bus& tool_command_bus(command_bus& local_bus) {
      return tool_bus(local_bus);
    }

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
        if (parsed.project.has_value()) {
          return build_project(ctx, *parsed.project, *parsed.output, { .strict = parsed.strict, .debug_info = !parsed.no_debug }, parsed.emit_oasm);
        }
        return build(ctx, parsed.inputs, *parsed.output, { .strict = parsed.strict, .debug_info = !parsed.no_debug }, parsed.emit_oasm);
      }
      if (parsed.command == "view" || parsed.command == "data") {
        if (parsed.output.has_value() || parsed.emit_oasm) {
          return tool_result::error(std::format("'{}' writes nothing, so it takes neither -o nor --emit-oasm\n{}", parsed.command, kUsage));
        }
        return view(ctx, parsed.command, parsed.inputs, { .strict = parsed.strict, .debug_info = !parsed.no_debug });
      }
      if (parsed.command == "check") {
        if (parsed.output.has_value() || parsed.emit_oasm || parsed.strict || parsed.no_debug) {
          return tool_result::error(std::format("'check' takes files and no options\n{}", kUsage));
        }
        return check(ctx, parsed.inputs);
      }
      if (parsed.command == "info") {
        if (parsed.inputs.size() != 1 || parsed.output.has_value() || parsed.strict || parsed.no_debug) {
          return tool_result::error(std::format("'info' takes one input and no options\n{}", kUsage));
        }
        return info(ctx, from_working_directory(ctx, parsed.inputs.front()));
      }
      return tool_result::error(std::format("unknown command '{}'\n{}", parsed.command, kUsage));
    }

  }  // namespace cli
}  // namespace other
