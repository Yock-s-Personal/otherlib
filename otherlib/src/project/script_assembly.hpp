/**
 * \file project/script_assembly.hpp
 * builds a project's C# assembly for a package: the project is built here, once, so the
 * packaged game loads the assembly as it is and needs no .NET SDK
 **/
#ifndef OTHERLIB_PROJECT_SCRIPT_ASSEMBLY_HPP
#define OTHERLIB_PROJECT_SCRIPT_ASSEMBLY_HPP

#include <string>
#include <string_view>

#include "core/defines.hpp"
#include "data-structures/std_container.hpp"

namespace other {

  struct script_assembly_request {
    filepath csproj;
    /// the .NET configuration: Debug or Release
    std::string configuration = "Release";
    /// the folder the assembly is copied into; made if it is not there
    filepath destination;
    /// also copy the .pdb. a Debug package wants it; a Release build writes one too, and it stays behind
    bool copy_symbols = false;
  };

  struct script_assembly_result {
    bool success = false;
    /// the copy in the destination folder
    filepath assembly;
    /// the compiler's errors when the build failed, otherwise why nothing was built or copied
    ostd::vector<std::string> errors;
  };

  /// where a build of csproj in that configuration leaves its assembly. the engine's projects
  ///  turn the target framework folder off, and the asset resolver assumes the same
  filepath built_assembly_path(const filepath& csproj, std::string_view configuration);

  /// always builds, whatever is already in bin/: a package must not ship a stale assembly or
  ///  one from the other configuration. runs 'dotnet build' and waits for it
  script_assembly_result build_script_assembly(const script_assembly_request& request);

}  // namespace other

#endif  // OTHERLIB_PROJECT_SCRIPT_ASSEMBLY_HPP
