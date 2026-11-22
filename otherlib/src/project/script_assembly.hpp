/**
 * \file project/script_assembly.hpp
 * builds a project's C# assembly for a package: the project is built here, once, so the
 * packaged game loads the assembly as it is and needs no .NET SDK
 **/
#ifndef OTHERLIB_PROJECT_SCRIPT_ASSEMBLY_HPP
#define OTHERLIB_PROJECT_SCRIPT_ASSEMBLY_HPP

#include <functional>
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
    /// asked while the build runs; true ends it, and the result says so in its errors
    std::function<bool()> should_stop = nullptr;
  };

  struct script_assembly_result {
    bool success = false;
    /// the copy in the destination folder
    filepath assembly;
    /// the compiler's errors when the build failed, otherwise why nothing was built or copied
    ostd::vector<std::string> errors;
  };

  /// where a package's build of csproj works: its output in bin/, its intermediate files in
  ///  obj/ and its error log beside them. it is under the project's obj/, which no package
  ///  takes and no C# project compiles from, and it is not the bin/ and obj/<configuration>
  ///  an open editor builds into and has loaded the project's assembly from
  filepath package_build_folder(const filepath& csproj, std::string_view configuration);

  /// always builds, whatever was built before: a package must not ship a stale assembly or
  ///  one from the other configuration. runs 'dotnet build' and waits for it
  script_assembly_result build_script_assembly(const script_assembly_request& request);

}  // namespace other

#endif  // OTHERLIB_PROJECT_SCRIPT_ASSEMBLY_HPP
