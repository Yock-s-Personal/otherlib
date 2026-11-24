/**
 * \file cli/tools/create_project.hpp
 **/
#ifndef OTHER_CLI_CLI_TOOLS_CREATE_PROJECT_HPP
#define OTHER_CLI_CLI_TOOLS_CREATE_PROJECT_HPP

#include "cli/tool.hpp"

namespace other {
  namespace cli {

    /// what a new project is made from
    struct new_project {
      /// starts with a letter; letters, digits, '-' and '_' only
      std::string name = "";
      /// the folder the project's own folder is made in
      filepath directory = "";
      std::string author = "N/A";
      std::string description = "N/A";
    };

    struct new_project_result {
      /// why nothing was made. none when the project was
      opt<std::string> error = std::nullopt;
      filepath project_file = "";
      /// every file that was written, the project file first
      std::vector<filepath> created = {};
      /// false when there was no environment to wire a C# script project to
      bool with_cs_project = false;
    };

    /// scaffolds a new editor-managed project: project file (toml), projectrc (lua),
    ///  starter scene, and a C# script project wired to the environment's assemblies.
    ///  nothing is left behind when it fails. it is what the create tool runs, and what an
    ///  editor calls to make a project
    new_project_result create_project(const new_project& request, const environment_paths& env);

    class create_project_tool final : public tool {
     public:
      create_project_tool() = default;
      ~create_project_tool() override = default;

      std::string_view name() const override { return "create"; }
      std::string_view summary() const override { return "Create a new Other Environment project"; }
      std::string_view usage() const override;

      tool_result execute(tool_context& ctx, std::span<const std::string> args) override;
    };

  }  // namespace cli
}  // namespace other

#endif  // OTHER_CLI_CLI_TOOLS_CREATE_PROJECT_HPP
