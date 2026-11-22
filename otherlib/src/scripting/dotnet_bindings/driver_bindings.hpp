/**
 * \file scripting/dotnet_bindings/driver_bindings.hpp
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_DRIVER_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_DRIVER_BINDINGS_HPP

#include "dotnet/native_string.hpp"
#include "dotnet/types.hpp"

namespace other {

  class driver;

  namespace detail {

    driver* get_dotnet_native_driver_unchecked();
    driver* get_dotnet_native_driver();
    void set_dotnet_native_driver(driver* drv);

  }  // namespace detail
  namespace bindings {

    int32_t native_driver_get_state();
    void native_driver_request_shutdown();
    const char* native_driver_get_project_name();

    float native_time_get_delta_time();
    float native_time_get_elapsed_time();
    int64_t native_time_get_frame_count();

    const char* native_config_get_string(const char* section, const char* key, const char* default_value);
    int32_t native_config_get_int(const char* section, const char* key, int32_t default_value);
    float native_config_get_float(const char* section, const char* key, float default_value);
    nbool32 native_config_get_bool(const char* section, const char* key, nbool32 default_value);

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_DRIVER_BINDINGS_HPP