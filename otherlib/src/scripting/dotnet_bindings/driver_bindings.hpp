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

    /// how long a script is told the frame took: what Time.DeltaTime gives
    float script_delta_time();

    /// \note FrameStat in C# is the same list in the same order
    enum class frame_stat : int32_t {
      FRAMES_A_SECOND,
      FRAME_MS,
      LONGEST_FRAME_MS,
      UPDATE_MS,
      RENDER_MS,
      WAIT_MS,
      ENGINE_SECONDS_A_SECOND,
      SCRIPT_SECONDS_A_SECOND,
      FIXED_STEPS_A_SECOND,
    };

    /// one number of the driver's frame statistics; 0 where there is no driver
    float native_frame_stats_value(int32_t which);
    /// the lengths of the last frames in milliseconds, oldest first. how many were written
    int32_t native_frame_stats_history(float* out_ms, int32_t capacity);

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