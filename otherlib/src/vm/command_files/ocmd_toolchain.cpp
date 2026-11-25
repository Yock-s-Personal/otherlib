/**
 * \file vm/command_files/ocmd_toolchain.cpp
 **/
#include "vm/command_files/ocmd_toolchain.hpp"

#include "core/logger.hpp"
#include "core/profiler.hpp"
#include "file/path_helpers.hpp"

#include "vm/code_generator_000.hpp"
#include "vm/command_bus.hpp"
#include "vm/command_files/asset_footprint.hpp"
#include "vm/command_files/lexer.hpp"
#include "vm/command_files/oasm_parser.hpp"
#include "vm/command_files/ocmd_compiler.hpp"
#include "vm/command_files/ocmd_linker.hpp"
#include "vm/command_language/command_assets.hpp"
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

    /// a file that runs is known by its ending. a file of declarations can have any other
    bool is_declarations_file(const filepath& path) {
      return path.extension() != ".ocmd";
    }

    /// a text is anything a script says, and only one that could be a path is looked for on the
    ///  disk. outside ASCII a path is turned to the system's code page, which throws on a byte
    ///  it has no letter for, so such a text is left as it is
    bool could_name_a_file(const std::string_view text) {
      constexpr size_t kLongest = 512;
      return !text.empty() && text.size() <= kLongest && std::ranges::none_of(text, [](const char c) -> bool {
        return c < 0x20 || c > 0x7e || c == '<' || c == '>' || c == '"' || c == '|' || c == '?' || c == '*';
      });
    }

    /// whether the file begins as only a file of declarations can: after any 'use' lines, with
    ///  'struct', 'enum', 'kind {' or 'kind name {'. this is what tells a 'let' given a path to
    ///  bring the file in, where a path to anything else stays a text
    bool begins_as_declarations(const filepath& path) {
      constexpr size_t kLooked = 4096;
      std::ifstream file(path, std::ios::binary);
      if (!file.is_open()) {
        return false;
      }
      std::string text(kLooked, '\0');
      file.read(text.data(), static_cast<std::streamsize>(kLooked));
      text.resize(static_cast<size_t>(file.gcount()));
      /// cut where a line ends, so the last token looked at is whole
      if (text.size() == kLooked) {
        text.resize(text.find_last_of('\n') == std::string::npos ? 0 : text.find_last_of('\n'));
      }

      /// whatever is wrong further in is the parser's to say, if the file is brought in
      diagnostic_engine unheard;
      command_lexer lexer{ text };
      const ostd::vector<command_token> tokens = lexer.tokenize(&unheard);
      size_t at = 0;
      const auto type_at = [&](const size_t index) -> command_token_type {
        return index < tokens.size() ? tokens[index].type : CMD_TOKEN_EOF;
      };
      while (type_at(at) == CMD_TOKEN_NEWLINE || type_at(at) == CMD_TOKEN_SEMICOLON || (type_at(at) == CMD_TOKEN_KW_USE && type_at(at + 1) == CMD_TOKEN_STRING)) {
        at += type_at(at) == CMD_TOKEN_KW_USE ? 2 : 1;
      }
      if (type_at(at) == CMD_TOKEN_KW_STRUCT || type_at(at) == CMD_TOKEN_KW_ENUM) {
        return true;
      }
      return type_at(at) == CMD_TOKEN_NAME && (type_at(at + 1) == CMD_TOKEN_LEFT_BRACE || (type_at(at + 1) == CMD_TOKEN_NAME && type_at(at + 2) == CMD_TOKEN_LEFT_BRACE));
    }

    /// the engine's own kinds, as the build names the file and where it is under the engine's root
    constexpr std::string_view kEngineKindsName = "engine://resources/ocmd/kinds.oass";
    constexpr std::string_view kEngineKindsPath = "resources/ocmd/kinds.oass";

    bool is_of_unknown_kind(const command_declaration& declaration, const std::span<const std::string_view> structs) {
      if (!std::ranges::contains(structs, std::string_view{ declaration.kind })) {
        return true;
      }
      return std::ranges::any_of(declaration.parts, [&](const command_declaration& part) { return is_of_unknown_kind(part, structs); });
    }

    /// whether something is declared as a kind that none of the files has a struct for
    bool declares_unknown_kind(const std::span<const command_unit> units) {
      ostd::vector<std::string_view> structs;
      for (const command_unit& unit : units) {
        for (const command_struct& declared : unit.file.structs) {
          structs.push_back(declared.name);
        }
      }
      const auto unknown = [&](const command_declaration& declaration) -> bool {
        return is_of_unknown_kind(declaration, structs);
      };
      for (const command_unit& unit : units) {
        if (std::ranges::any_of(unit.file.declarations, unknown)) {
          return true;
        }
        for (const command_stmt& stmt : unit.file.statements) {
          if (std::ranges::any_of(stmt.declared, unknown)) {
            return true;
          }
        }
      }
      return false;
    }

    /// whether the OASM made from a file has symbols of its own: a block, or a field of data.
    ///  the entry file has its entry block whatever it holds
    bool has_code(const command_file& file) {
      return !file.functions.empty() || !file.hooks.empty() || !file.statements.empty();
    }

    /// holds every unit's declarations to their structs. checker is left knowing the build
    bool check_units(command_declaration_checker& checker, std::span<const command_unit> units, diagnostic_engine* diag) {
      /// a struct, an enum or a declaration of one file can be named in any other
      for (const command_unit& unit : units) {
        checker.add(unit.file);
      }

      bool success = true;
      for (const command_unit& unit : units) {
        command_error_sink sink{ unit.name };
        const scoped_diagnostic_sink sink_scope(diag, "command-error-sink", &sink);
        bool passed = checker.check(unit.file, diag, unit.included);

        /// a variable is one thing, so the file it is given declares one
        for (const command_stmt& stmt : unit.file.statements) {
          if (stmt.included.empty()) {
            continue;
          }
          const auto file = std::ranges::find(units, stmt.included, &command_unit::name);
          const size_t declared = file != units.end() ? file->file.declarations.size() : 0;
          if (declared != 1) {
            diag->emit({ .severity = VM_DIAGNOSTIC_ERROR,
                         .error_code = SEMA_BAD_DECLARATION,
                         .phase = VM_PHASE_SEMA,
                         .span = stmt.span,
                         .final_message = std::format("'{}' is given the file '{}', which declares {} things; a file given to a variable declares one, and a file of several is brought in with 'use'", stmt.name, stmt.included, declared) });
            passed = false;
          }
        }

        if (!passed) {
          CORE_LOG_ERROR("[VM] : '{}' has errors", unit.name);
          success = false;
        }
      }
      return success;
    }

    /// where the first thing a file declares is, if it declares anything
    opt<source_span> first_declared(const command_file& file) {
      if (!file.enums.empty()) {
        return file.enums.front().span;
      }
      if (!file.structs.empty()) {
        return file.structs.front().span;
      }
      if (!file.declarations.empty()) {
        return file.declarations.front().span;
      }
      for (const command_stmt& stmt : file.statements) {
        if (!stmt.declared.empty() || !stmt.included.empty()) {
          return stmt.span;
        }
      }
      return std::nullopt;
    }

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

  opt<command_file> ocmd_toolchain::parse_command_source(const std::string_view name, const std::string_view text, diagnostic_engine* diag, const bool declarations_only) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine cannot be null");
    PROFILE_SECTION("ocmd_toolchain::parse_command_source");

    command_error_sink sink{ name };
    const scoped_diagnostic_sink sink_scope(diag, "command-error-sink", &sink);

    command_lexer lexer{ text };
    const ostd::vector<command_token> tokens = lexer.tokenize(diag);
    if (lexer.failed()) {
      return std::nullopt;
    }
    command_file file = command_parser{ tokens, { .declarations_only = declarations_only } }.parse(diag);
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
      bool included = false;
    };
    /// a 'let' that was given a file, to be told the file's name in the build once all are read
    struct inclusion {
      size_t unit = 0;
      size_t statement = 0;
      filepath file;
    };
    ostd::vector<pending_file> pending = { { .path = root, .name = std::string{ root_name } } };
    /// seen[i] is the file units[i] was read from
    ostd::vector<filepath> seen;
    ostd::vector<command_unit> units;
    ostd::vector<inclusion> inclusions;

    std::error_code cwd_error;
    const filepath started_in = std::filesystem::current_path(cwd_error);

    size_t next = 0;
    bool engine_kinds_tried = false;
    while (true) {
      /// pending grows while it is walked: each file adds the files it uses
      for (; next < pending.size(); ++next) {
        const pending_file current = pending[next];
        std::error_code ec;
        const filepath canonical = std::filesystem::weakly_canonical(current.path, ec);
        if (const auto known = std::ranges::find(seen, canonical); known != seen.end()) {
          units[static_cast<size_t>(known - seen.begin())].included |= current.included;
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

        opt<command_file> parsed = parse_command_source(current.name, text.str(), diag, current.included || is_declarations_file(current.path));
        if (!parsed.has_value()) {
          CORE_LOG_ERROR("[VM] : '{}' has errors and was not built", current.name);
          return {};
        }

        /// a 'use' path is relative to the file that names it
        for (const command_use& use : parsed->uses) {
          pending.push_back({ .path = current.path.parent_path() / use.path, .name = (filepath{ current.name }.parent_path() / use.path).generic_string() });
        }

        /// a variable given the path of a file of declarations is given what the file declares.
        ///  the file is looked for beside this one, and then from where the build was started
        for (size_t i = 0; i < parsed->statements.size(); ++i) {
          const command_stmt& stmt = parsed->statements[i];
          if (stmt.kind != CMD_STMT_LET || stmt.exprs.size() != 1 || stmt.exprs.front().kind != CMD_EXPR_STRING || !could_name_a_file(stmt.exprs.front().text)) {
            continue;
          }
          const filepath named = stmt.exprs.front().text;
          const filepath beside = current.path.parent_path() / named;
          const bool is_beside = std::filesystem::is_regular_file(beside, ec);
          const filepath found = is_beside ? beside : started_in / named;
          if ((!is_beside && !std::filesystem::is_regular_file(found, ec)) || !begins_as_declarations(found)) {
            continue;
          }
          const std::string name = is_beside ? (filepath{ current.name }.parent_path() / named).lexically_normal().generic_string() : named.lexically_normal().generic_string();
          pending.push_back({ .path = found, .name = name, .included = true });
          inclusions.push_back({ .unit = units.size(), .statement = i, .file = std::filesystem::weakly_canonical(found, ec) });
        }
        units.push_back({ .name = current.name, .file = std::move(*parsed), .included = current.included });
      }

      /// the engine's own kinds are in a build without being asked for, once something in it
      ///  is declared as a kind that no file it has reached knows. a build that has every
      ///  kind it uses is left as it is
      if (engine_kinds_tried || !declares_unknown_kind(units)) {
        break;
      }
      engine_kinds_tried = true;
      const filepath engine_kinds = environment_root() / kEngineKindsPath;
      std::error_code ec;
      if (!std::filesystem::is_regular_file(engine_kinds, ec)) {
        break;
      }
      pending.push_back({ .path = engine_kinds, .name = std::string{ kEngineKindsName } });
    }

    /// one file can be reached by two paths, and is in the build by the first
    for (const inclusion& made : inclusions) {
      const auto file = std::ranges::find(seen, made.file);
      OTHER_ASSERT(file != seen.end(), "An included file was not read: {}", made.file.string());
      units[made.unit].file.statements[made.statement].included = units[static_cast<size_t>(file - seen.begin())].name;
    }
    return units;
  }

  bool ocmd_toolchain::check_command_units(std::span<const command_unit> units, diagnostic_engine* diag, command_check_counts* counts) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine cannot be null");
    PROFILE_SECTION("ocmd_toolchain::check_command_units");

    command_declaration_checker checker;
    const bool success = check_units(checker, units, diag);
    if (counts != nullptr) {
      *counts = checker.counts();
    }
    return success;
  }

  ostd::vector<oasm_source> ocmd_toolchain::lower_command_units(const command_bus& bus, std::span<const command_unit> units, diagnostic_engine* diag, ocmd_link_options options) {
    return lower_command_units(bus, units, diag, { .has_entry_unit = true, .strict = options.strict }, nullptr);
  }

  ostd::vector<oasm_source> ocmd_toolchain::lower_command_units(const command_bus& bus, std::span<const command_unit> units, diagnostic_engine* diag,
                                                                const command_build_options& options, ostd::vector<command_hook_record>* hooks) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine cannot be null");
    PROFILE_SECTION("ocmd_toolchain::lower_command_units");

    const auto stem_of = [&options](const command_unit& unit) -> std::string {
      return options.name_by_path ? command_lowering::unit_path_stem(unit.name) : command_lowering::unit_stem(unit.name);
    };

    /// every declaration outside a block is an asset, written as an asset block of the unit it
    ///  is in. the blocks are written from the declarations and their structs together, so the
    ///  build is held to its structs first
    command_declaration_checker declarations;
    const bool declares = std::ranges::any_of(units, [](const command_unit& unit) { return first_declared(unit.file).has_value(); });
    if (declares && !check_units(declarations, units, diag)) {
      return {};
    }
    const command_asset_lowering asset_lowering{ declarations };

    /// a variable given a file stands for the asset that file declares
    ostd::unordered_map<std::string, std::string> included;
    for (const command_unit& unit : units) {
      if (unit.included && unit.file.declarations.size() == 1) {
        included[unit.name] = command_asset_lowering::declared_name(unit.file.declarations.front(), unit.name);
      }
    }

    struct named_asset {
      std::string kind;
      std::string name;
    };
    ostd::vector<named_asset> assets;

    ostd::vector<oasm_source> sources;
    sources.reserve(units.size());
    bool success = true;
    for (size_t i = 0; i < units.size(); ++i) {
      /// generated names carry the unit's stem, so two units with one stem would define the same
      ///  symbols. a file of declarations alone generates none
      const std::string stem = stem_of(units[i]);
      const auto has_symbols = [&](const size_t index) -> bool {
        return (options.has_entry_unit && index == 0) || has_code(units[index].file);
      };
      bool stem_taken = false;
      for (size_t earlier = 0; earlier < i && has_symbols(i); ++earlier) {
        stem_taken = stem_taken || (has_symbols(earlier) && stem_of(units[earlier]) == stem);
      }
      if (stem_taken) {
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
      const command_lowering_options lowering_options = { .entry_unit = options.has_entry_unit && i == 0, .strict = options.strict, .stem = stem, .included = included };
      command_lowering_result lowered = command_lowering{ bus, units[i].name, imported, lowering_options }.lower(units[i].file, diag);
      if (!lowered.success) {
        CORE_LOG_ERROR("[VM] : '{}' has errors and was not built", units[i].name);
        success = false;
        continue;
      }
      if (hooks != nullptr) {
        hooks->append_range(std::move(lowered.hooks));
      }

      /// the asset blocks go after the code, which keeps the lines the origins count
      const auto add_asset = [&](const command_declaration& declaration, const std::string& name) -> void {
        const auto same = [&](const named_asset& earlier) -> bool {
          return earlier.kind == declaration.kind && earlier.name == name;
        };
        if (std::ranges::any_of(assets, same)) {
          diag->emit({ .severity = VM_DIAGNOSTIC_ERROR,
                       .error_code = SEMA_DUPLICATE_NAME,
                       .phase = VM_PHASE_SEMA,
                       .span = declaration.span,
                       .final_message = std::format("this build already has a {} called '{}'; one with no name of its own is called after its variable or its file", declaration.kind, name) });
          success = false;
          return;
        }
        assets.push_back({ .kind = declaration.kind, .name = name });
        const std::string block = asset_block_text(asset_lowering.lower(declaration, name));
        /// an origin for each line: the empty one, the block's first, which is the declaration's, and the rest
        lowered.origins.push_back({});
        lowered.origins.push_back(declaration.span.start);
        lowered.origins.resize(lowered.origins.size() + static_cast<size_t>(std::ranges::count(block, '\n')) - 1);
        lowered.oasm += std::format("\n{}", block);
      };
      for (const command_declaration& declaration : units[i].file.declarations) {
        add_asset(declaration, command_asset_lowering::declared_name(declaration, units[i].name));
      }
      for (const command_stmt& stmt : units[i].file.statements) {
        if (!stmt.declared.empty()) {
          add_asset(stmt.declared.front(), command_asset_lowering::bound_name(stmt));
        }
      }
      sources.push_back({ .name = units[i].name, .text = std::move(lowered.oasm), .origins = std::move(lowered.origins) });
    }

    if (!success) {
      sources.clear();
    }
    return sources;
  }

  ostd::vector<ocmd_unit> ocmd_toolchain::compile_units(std::span<const oasm_source> sources, diagnostic_engine* diag) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine cannot be null");
    PROFILE_SECTION("ocmd_toolchain::compile_units");

    ostd::vector<ocmd_unit> units;
    units.reserve(sources.size());
    for (const oasm_source& source : sources) {
      /// stops at the first file that fails; its errors have already gone to diag
      try {
        units.push_back({ .name = source.name, .program = ocmd_compiler{}.compile(source.text, make_scope<code_generator_000>(), diag) });
        /// the compiler counted lines in generated text; what a person reads is the file it came from
        if (!source.origins.empty()) {
          for (compiled_code_block& block : units.back().program.compiled_blocks) {
            for (source_location& origin : block.artifact.origins) {
              origin = origin.line >= 1 && origin.line <= source.origins.size() ? source.origins[origin.line - 1] : source_location{};
            }
          }
          for (source_location& origin : units.back().program.asset_origins) {
            origin = origin.line >= 1 && origin.line <= source.origins.size() ? source.origins[origin.line - 1] : source_location{};
          }
        }
      } catch (const std::exception& e) {
        CORE_LOG_ERROR("[VM] : failed to compile '{}': {}", source.name, e.what());
        return {};
      } catch (...) {
        CORE_LOG_ERROR("[VM] : failed to compile '{}': unknown exception", source.name);
        return {};
      }
    }
    return units;
  }

  obin_build_result ocmd_toolchain::link_units(const command_bus& bus, std::span<const ocmd_unit> units, diagnostic_engine* diag, ocmd_link_options options) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine cannot be null");
    PROFILE_SECTION("ocmd_toolchain::link_units");

    obin_build_result result;
    try {
      result.bytes = ocmd_linker{ units, options }.link(bus.create_default_symbol_resolver(), diag);
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

  obin_build_result ocmd_toolchain::build_obin(const command_bus& bus, std::span<const oasm_source> sources, diagnostic_engine* diag, ocmd_link_options options) {
    OTHER_ASSERT(diag != nullptr, "Diagnostic engine cannot be null");
    PROFILE_SECTION("ocmd_toolchain::build_obin");

    if (sources.empty()) {
      CORE_LOG_ERROR("[VM] : nothing to build, no sources were given");
      return {};
    }
    const ostd::vector<ocmd_unit> units = compile_units(sources, diag);
    if (units.empty()) {
      return {};
    }
    return link_units(bus, units, diag, options);
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

    /// with a debug section or an asset section the file is longer than the program; without
    ///  either it is exactly the program
    const bool has_debug_section = header.prog_header.has_debug_flag != 0;
    const bool has_asset_section = header.prog_header.has_assets_flag != 0;
    const size_t bytes_after_header = bytes.size() - sizeof(ocmd_file_header);
    const size_t program_size = header.prog_header.program_size;
    const size_t code_start = header.prog_header.code_section_offset;
    const size_t data_start = header.prog_header.data_section_offset;
    const size_t entry_point = header.prog_header.entry_point_address;
    if (has_debug_section || has_asset_section ? program_size > bytes_after_header : program_size != bytes_after_header) {
      return std::format("its header counts {} program bytes and the file holds {}", program_size, bytes_after_header);
    }
    if (has_debug_section && !obin_debug_info::decode(obin_debug_bytes(bytes)).has_value()) {
      return "its debug section is cut short or was written by tools this build does not match";
    }
    if (has_asset_section) {
      const opt<obin_assets> assets = obin_assets::decode(obin_asset_bytes(bytes));
      if (!assets.has_value()) {
        return "its asset section is cut short, was changed after it was written, or was written by tools this build does not match";
      }
      if (!std::ranges::all_of(assets->assets, [](const obin_asset& asset) { return read_asset_footprint(asset.footprint).has_value(); })) {
        return "one of its assets has a footprint this build cannot read";
      }
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