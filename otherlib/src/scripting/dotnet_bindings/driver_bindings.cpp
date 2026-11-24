/**
 * \file scripting/dotnet_bindings/driver_bindings.cpp
 **/
#include "scripting/dotnet_bindings/driver_bindings.hpp"

#include "core/logger.hpp"

#include "driver/driver.hpp"
#include "scripting/main_thread_only.hpp"

namespace other {
  namespace bindings {
    namespace {
      static driver* s_driver = nullptr;
    }  // namespace
  }  // namespace bindings
  namespace detail {

    driver* get_dotnet_native_driver_unchecked() {
      return bindings::s_driver;
    }

    driver* get_dotnet_native_driver() {
      OTHER_ASSERT(bindings::s_driver != nullptr, "Driver pointer is null.");
      return bindings::s_driver;
    }

    void set_dotnet_native_driver(driver* drv) {
      OTHER_ASSERT(drv != nullptr, "Driver pointer is null.");
      bindings::s_driver = drv;
    }

  }  // namespace detail
  namespace bindings {

    int32_t native_driver_get_state() {
      OTHER_ASSERT(s_driver != nullptr, "Driver pointer is null.");
      return static_cast<int32_t>(s_driver->current_driver_state());
    }

    void native_driver_request_shutdown() {
      OTHER_ASSERT(s_driver != nullptr, "Driver pointer is null.");
      s_driver->request_shutdown();
    }

    const char* native_driver_get_project_name() {
      OTHER_ASSERT(s_driver != nullptr, "Driver pointer is null.");
      std::string project_name = s_driver->get_project_name();
      return returned_string(project_name);
    }

    float script_delta_time() {
      /// the frame's length as the driver has it, which is what the scene is ticked with.
      ///  it was ImGui's, and a driver that draws no UI never starts an ImGui frame: its
      ///  scripts were told a sixtieth of a second whatever the frame took
      if (s_driver != nullptr) {
        return static_cast<float>(s_driver->frame_seconds());
      }
      /// no driver, as in a test that draws: what the UI says
      return ImGui::GetCurrentContext() != nullptr ? ImGui::GetIO().DeltaTime : 0.f;
    }

    float native_time_get_delta_time() {
      return script_delta_time();
    }

    float native_frame_stats_value(int32_t which) {
      if (s_driver == nullptr) {
        return 0.f;
      }
      const frame_statistics::summary& s = s_driver->get_frame_statistics().latest();
      switch (static_cast<frame_stat>(which)) {
        case frame_stat::FRAMES_A_SECOND: return s.frames_a_second;
        case frame_stat::FRAME_MS: return s.frame_ms;
        case frame_stat::LONGEST_FRAME_MS: return s.longest_frame_ms;
        case frame_stat::UPDATE_MS: return s.update_ms;
        case frame_stat::RENDER_MS: return s.render_ms;
        case frame_stat::WAIT_MS: return s.wait_ms;
        case frame_stat::ENGINE_SECONDS_A_SECOND: return s.engine_seconds_a_second;
        case frame_stat::SCRIPT_SECONDS_A_SECOND: return s.script_seconds_a_second;
        case frame_stat::FIXED_STEPS_A_SECOND: return s.fixed_steps_a_second;
        default: return 0.f;
      }
    }

    int32_t native_frame_stats_history(float* out_ms, int32_t capacity) {
      if (s_driver == nullptr || out_ms == nullptr || capacity <= 0) {
        return 0;
      }
      return static_cast<int32_t>(s_driver->get_frame_statistics().copy_history(std::span<float>{ out_ms, static_cast<size_t>(capacity) }));
    }

    float native_time_get_elapsed_time() {
      /// the driver's own count, for the reason its frame length is: ImGui's stood still
      ///  in a driver that draws no UI
      if (s_driver != nullptr) {
        return static_cast<float>(s_driver->elapsed_seconds());
      }
      return ImGui::GetCurrentContext() != nullptr ? static_cast<float>(ImGui::GetTime()) : 0.f;
    }

    int64_t native_time_get_frame_count() {
      if (s_driver != nullptr) {
        return static_cast<int64_t>(s_driver->frame_number());
      }
      return ImGui::GetCurrentContext() != nullptr ? ImGui::GetFrameCount() : 0;
    }

    const char* native_config_get_string(const char* section, const char* key, const char* default_value) {
      /// \todo connect to config table
      return default_value;
    }

    int32_t native_config_get_int(const char* section, const char* key, int32_t default_value) {
      return default_value;
    }

    float native_config_get_float(const char* section, const char* key, float default_value) {
      return default_value;
    }

    nbool32 native_config_get_bool(const char* section, const char* key, nbool32 default_value) {
      return default_value;
    }

  }  // namespace bindings
}  // namespace other