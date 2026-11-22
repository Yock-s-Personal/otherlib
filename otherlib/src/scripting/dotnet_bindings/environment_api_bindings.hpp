/**
 * \file scripting/dotnet_bindings/environment_api_bindings.hpp
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_ENVIRONMENT_API_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_ENVIRONMENT_API_BINDINGS_HPP

#include "dotnet/native_string.hpp"
#include "dotnet/types.hpp"

namespace other {
  namespace bindings {

    const char* native_get_program_files_folder(const char* app_name_str);
    const char* native_get_app_data_folder(const char* app_name_str, nbool32 create_flag);
    const char* native_get_install_folder();

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_ENVIRONMENT_API_BINDINGS_HPP