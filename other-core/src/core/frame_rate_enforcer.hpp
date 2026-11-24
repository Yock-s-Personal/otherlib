/**
 * \file core/frame_rate_enforcer.hpp
 **/
#ifndef OTHER_CORE_CORE_FRAME_RATE_ENFORCER_HPP
#define OTHER_CORE_CORE_FRAME_RATE_ENFORCER_HPP

#include <thread>

#include "core/profiler.hpp"
#include "core/time.hpp"

namespace other {

  /// holds a loop to a number of frames a second, by sleeping what is left of each frame.
  ///  a rate of 0 or less holds nothing
  struct frame_rate_enforcer {
    explicit frame_rate_enforcer(integer_t frames_a_second)
        : frame_duration(frames_a_second > 0 ? static_cast<natural_t>(1000 / frames_a_second) : 0) {}

    void wait() {
      if (frame_duration == 0) {
        return;
      }
      PROFILE_SECTION("frame_rate_enforcer::wait");
      auto now = std::chrono::steady_clock::now();
      if (last_frame_time.time_since_epoch().count() == 0) {
        last_frame_time = now;
        return;
      }

      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_frame_time).count();
      if (elapsed < static_cast<int64_t>(frame_duration)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(frame_duration) - elapsed));
      }
      last_frame_time = std::chrono::steady_clock::now();
    }

   private:
    /// ms
    natural_t frame_duration = 0;
    std::chrono::time_point<std::chrono::steady_clock> last_frame_time;
  };

}  // namespace other

#endif  // OTHER_CORE_CORE_FRAME_RATE_ENFORCER_HPP