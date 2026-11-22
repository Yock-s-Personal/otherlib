/**
 * \file scripting/dotnet_bindings/environment_api_bindings.cpp
 **/
#include "scripting/dotnet_bindings/environment_api_bindings.hpp"

#include "core/defines.hpp"
#include "core/profiler.hpp"

#include "scripting/main_thread_only.hpp"

namespace other {
  namespace bindings {

    const char* native_get_program_files_folder(const char* app_name_str) {
      PROFILE_SECTION("native_get_program_files_folder");
      std::string app_name = app_name_str;
      filepath program_files_folder = get_program_files_folder(app_name);
      return returned_string(program_files_folder.string());
    }

    const char* native_get_app_data_folder(const char* app_name_str, nbool32 create_flag) {
      PROFILE_SECTION("native_get_app_data_folder");
      std::string app_name = app_name_str;
      filepath app_data_folder = get_app_data_folder(app_name, create_flag);
      return returned_string(app_data_folder.string());
    }

    const char* native_get_install_folder() {
      PROFILE_SECTION("native_get_install_folder");
      filepath install_folder = get_other_environment_install_folder();
      return returned_string(install_folder.string());
    }

  }  // namespace bindings
}  // namespace other