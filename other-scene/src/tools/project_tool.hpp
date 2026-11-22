/**
 * \file tools/project_tool.hpp
 **/
#ifndef OTHER_SCENE_TOOLS_PROJECT_TOOL_HPP
#define OTHER_SCENE_TOOLS_PROJECT_TOOL_HPP

#include "core/coroutine.hpp"

#include "tools/tool.hpp"

namespace other {

  class project_tool : public tool {
   public:
    project_tool()
        : tool("Other.Toolset.ProjectTool") {}
    ~project_tool() override = default;

    void generate_dotnet_project(const filepath& csproj_path);

    /// runs 'dotnet build' on a csproj in the engine's own configuration and waits for it:
    ///  the build's exit code, or -1 when it never started (the build log says why). no
    ///  managed code: it runs on a worker thread, and managed code is main thread only
    static int32_t build_project(const filepath& csproj_path);

    const filepath& get_dotnet_project_path() const { return dotnet_project_path; }

    /// errors-only msbuild log of a csproj's last build; under obj/, which the asset watcher skips
    static filepath build_log_path(const filepath& csproj_path);
    /// empty when the last build wrote no errors or never ran
    static std::string read_build_log(const filepath& csproj_path);

   private:
    filepath dotnet_project_path;
  };

}  // namespace other

#endif  // OTHER_SCENE_TOOLS_PROJECT_TOOL_HPP