/**
 * \file project/script_assembly.cpp
 **/
#include "project/script_assembly.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "cli/process.hpp"

namespace other {
  namespace {

    script_assembly_result failed(std::string reason) {
      CORE_LOG_ERROR("[PROJECT] : {}", reason);
      script_assembly_result result;
      result.errors.push_back(std::move(reason));
      return result;
    }

    /// one entry per line of the build's error log, without the '   1:7>' msbuild puts in
    ///  front of each to say which of its nodes wrote it
    ostd::vector<std::string> lines_of(const filepath& log_path) {
      ostd::vector<std::string> lines;
      std::ifstream stream(log_path, std::ios::binary);
      for (std::string line; std::getline(stream, line);) {
        if (!line.empty() && line.back() == '\r') {
          line.pop_back();
        }
        /// msbuild opens a utf-8 log with a byte order mark
        if (line.starts_with("\xEF\xBB\xBF")) {
          line.erase(0, 3);
        }
        const size_t node_end = line.find_first_not_of(" 0123456789:");
        if (node_end != std::string::npos && node_end > 0 && line[node_end] == '>') {
          line.erase(0, node_end + 1);
        }
        if (!line.empty()) {
          lines.push_back(line);
        }
      }
      return lines;
    }

  }  // namespace

  filepath package_build_folder(const filepath& csproj, const std::string_view configuration) {
    return csproj.parent_path() / "obj" / "package" / configuration;
  }

  script_assembly_result build_script_assembly(const script_assembly_request& request) {
    PROFILE_SECTION("build_script_assembly");
    if (request.configuration != "Debug" && request.configuration != "Release") {
      return failed(std::format("'{}' is not a configuration a script assembly is built in; it is Debug or Release", request.configuration));
    }

    std::error_code ec;
    const filepath csproj = std::filesystem::absolute(request.csproj, ec);
    if (ec || !std::filesystem::is_regular_file(csproj, ec)) {
      return failed(std::format("C# project '{}' does not exist", request.csproj.string()));
    }

    const opt<filepath> dotnet = cli::find_program_on_path("dotnet");
    if (!dotnet.has_value()) {
      return failed("the .NET SDK was not found: 'dotnet' is not on the PATH");
    }

    /// an errors-only log, as the editor's builds write one. removed first: a build that never
    ///  starts must not leave the last build's errors behind
    const filepath build_folder = package_build_folder(csproj, request.configuration);
    const filepath output = build_folder / "bin";
    const filepath log_path = build_folder / "build-errors.log";
    std::filesystem::create_directories(build_folder, ec);
    std::filesystem::remove(log_path, ec);

    const cli::process_launch launch = {
      .executable = *dotnet,
      .arguments = {
        "build",
        csproj.string(),
        "-c",
        request.configuration,
        /// both of this build's own. left where they are by default, the build would replace
        ///  the assembly an open editor has loaded, and the editor would take that for a change
        ///  to load
        "-o",
        output.string(),
        /// msbuild wants this one to end in a separator
        std::format("-p:IntermediateOutputPath={}/", (build_folder / "obj").generic_string()),
        "--no-incremental",
        "-nologo",
        /// nothing on the console: msbuild prints a summary there whatever the verbosity, and
        ///  the errors are read from the file log below
        "-noconlog",
        std::format("-flp:logfile={};errorsonly;encoding=UTF-8", log_path.string()),
      },
      .working_directory = csproj.parent_path(),
      .wait_for_exit = true,
      .new_console = false,
    };
    CORE_LOG_INFO("[PROJECT] : building '{}' ({})", csproj.string(), request.configuration);
    const cli::process_result built = cli::launch_process(launch);
    if (!built.started) {
      return failed(std::format("could not start 'dotnet build': {}", built.error));
    }
    if (built.exit_code != 0) {
      script_assembly_result result;
      result.errors = lines_of(log_path);
      if (result.errors.empty()) {
        result.errors.push_back(std::format("'dotnet build' of '{}' failed with exit code {}", csproj.string(), built.exit_code));
      }
      for (const std::string& error : result.errors) {
        CORE_LOG_ERROR("[PROJECT] : {}", error);
      }
      return result;
    }

    const filepath built_assembly = output / (csproj.stem().string() + ".dll");
    if (!std::filesystem::is_regular_file(built_assembly, ec)) {
      return failed(std::format("the build succeeded but '{}' is not there; the project's assembly must have the name of its project file", built_assembly.string()));
    }

    std::filesystem::create_directories(request.destination, ec);
    const filepath copied = request.destination / built_assembly.filename();
    std::filesystem::copy_file(built_assembly, copied, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
      return failed(std::format("could not copy '{}' to '{}': {}", built_assembly.string(), copied.string(), ec.message()));
    }

    filepath symbols = built_assembly;
    symbols.replace_extension(".pdb");
    if (request.copy_symbols && std::filesystem::is_regular_file(symbols, ec)) {
      std::filesystem::copy_file(symbols, request.destination / symbols.filename(), std::filesystem::copy_options::overwrite_existing, ec);
      if (ec) {
        return failed(std::format("could not copy '{}' to '{}': {}", symbols.string(), request.destination.string(), ec.message()));
      }
    }

    return { .success = true, .assembly = copied };
  }

}  // namespace other
