/**
 * \file tools/project_tool.cpp
 **/
#include "tools/project_tool.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "core/profiler.hpp"

#include "cli/process.hpp"

namespace other {
  namespace {

    /// no process means no msbuild log, so the reason goes where the engine looks for errors
    int32_t build_never_started(const filepath& log_path, const std::string_view reason) {
      const std::string message = std::format("Could not start 'dotnet build': {}", reason);
      CORE_LOG_ERROR("{}", message);
      std::ofstream log(log_path, std::ios::binary | std::ios::trunc);
      log.write(message.data(), static_cast<std::streamsize>(message.size()));
      return -1;
    }

  }  // namespace

  int32_t project_tool::build_project(const filepath& csproj_path) {
    OTHER_ASSERT(!csproj_path.empty(), "Dotnet project path is empty in build_project.");
    OTHER_ASSERT(std::filesystem::exists(csproj_path), "Dotnet project file '{}' does not exist.", csproj_path.string());
    PROFILE_SECTION("project_tool::build_project");

    /// removed first: a build that never starts must not leave the previous build's errors behind
    const filepath log_path = build_log_path(csproj_path);
    std::error_code ec;
    std::filesystem::create_directories(log_path.parent_path(), ec);
    std::filesystem::remove(log_path, ec);

    const opt<filepath> dotnet = cli::find_program_on_path("dotnet");
    if (!dotnet.has_value()) {
      return build_never_started(log_path, "the .NET SDK was not found, 'dotnet' is not on the PATH");
    }

    const cli::process_result built = cli::launch_process({
      .executable = *dotnet,
      .arguments = {
        "build",
        csproj_path.string(),
        "-c",
        get_project_build_config_string(),
        /// a file logger, not redirected output: build servers outlive the build and would hold a pipe open
        std::format("-flp:logfile={};errorsonly;encoding=UTF-8", log_path.string()),
      },
      .wait_for_exit = true,
      .new_console = false,
    });
    if (!built.started) {
      return build_never_started(log_path, built.error);
    }
    return built.exit_code;
  }

  filepath project_tool::build_log_path(const filepath& csproj_path) {
    return csproj_path.parent_path() / "obj" / (csproj_path.stem().string() + ".build-errors.log");
  }

  std::string project_tool::read_build_log(const filepath& csproj_path) {
    PROFILE_SECTION("project_tool::read_build_log");
    std::ifstream file(build_log_path(csproj_path), std::ios::binary);
    if (!file.is_open()) {
      return {};
    }

    std::string contents{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    /// msbuild opens a utf-8 log with a byte order mark
    if (contents.starts_with("\xEF\xBB\xBF")) {
      contents.erase(0, 3);
    }
    return contents;
  }

}  // namespace other