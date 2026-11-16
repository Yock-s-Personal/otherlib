/**
 * \file tools/project_tool.cpp
 **/
#include "tools/project_tool.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "core/profiler.hpp"
#include "dotnet/native_string.hpp"

namespace other {

  void project_tool::generate_dotnet_project(const filepath& csproj_path) {
    OTHER_ASSERT(!csproj_path.empty(), "Csproj path is empty in generate_dotnet_project.");
    OTHER_ASSERT(!std::filesystem::exists(csproj_path), "Csproj file '{}' already exists in generate_dotnet_project.", csproj_path.string());
    PROFILE_SECTION("project_tool::generate_dotnet_project");

    native_string path = native_string::new_str(csproj_path.string());
    invoke_tool_method("CreateDefaultCsproj", path);
    native_string::free_str(path);

    dotnet_project_path = csproj_path;
  }

  void project_tool::start_project_build(const filepath& csproj_path) {
    OTHER_ASSERT(!csproj_path.empty(), "Dotnet project path is empty in start_project_build.");
    OTHER_ASSERT(std::filesystem::exists(csproj_path), "Dotnet project file '{}' does not exist.", csproj_path.string());
    PROFILE_SECTION("project_tool::start_project_build");

    /// removed first: a build that never starts must not leave the previous build's errors behind
    const filepath log_path = build_log_path(csproj_path);
    std::error_code ec;
    std::filesystem::create_directories(log_path.parent_path(), ec);
    std::filesystem::remove(log_path, ec);

    std::string build_config = get_project_build_config_string();
    native_string path = native_string::new_str(csproj_path.string());
    native_string config = native_string::new_str(build_config);
    native_string log = native_string::new_str(log_path.string());

    invoke_tool_method("SetConfig", config);
    invoke_tool_method("SetCsprojFilePath", path);
    invoke_tool_method("SetBuildLogPath", log);
    invoke_tool_method("StartCsprojBuild");

    native_string::free_str(log);
    native_string::free_str(config);
    native_string::free_str(path);

    dotnet_project_path = csproj_path;
  }

  bool project_tool::project_build_in_progress() const {
    return invoke_tool_method<nbool32>("IsBuildInProgress");
  }

  void project_tool::cleanup_build() {
    invoke_tool_method("CleanupBuild");
  }

  int32_t project_tool::get_build_result() const {
    return invoke_tool_method<int32_t>("GetBuildResult");
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