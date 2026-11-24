/**
 * \file driver/frame_statistics.hpp
 * what the driver's loop did, frame by frame: how long each frame took and where the time
 * went, and how much time the scene and its scripts were told had passed. it is what a
 * profiling window draws, and what a driver with no window can write to its log
 **/
#ifndef OTHERLIB_DRIVER_FRAME_STATISTICS_HPP
#define OTHERLIB_DRIVER_FRAME_STATISTICS_HPP

#include <array>
#include <span>

#include "core/defines.hpp"

namespace other {

  class frame_statistics {
   public:
    /// how many frames back the history goes
    constexpr static size_t kHistory = 240;
    /// how long is added up before the summary is made again, in seconds
    constexpr static double kSummaryInterval = 0.5;

    /// one frame, in seconds of the clock but for the last three
    struct frame_times {
      /// from the start of one frame to the start of the next
      double frame = 0.0;
      double update = 0.0;
      double render = 0.0;
      /// what the loop slept to hold its frame rate
      double wait = 0.0;
      /// how much time the engine told the scene had passed
      double engine = 0.0;
      /// how much time a script was told had passed (Time.DeltaTime)
      double script = 0.0;
      uint32_t fixed_steps = 0;
    };

    /// the frames of the last interval. times are the average frame's, in milliseconds
    struct summary {
      float frames_a_second = 0.f;
      float frame_ms = 0.f;
      float longest_frame_ms = 0.f;
      float update_ms = 0.f;
      float render_ms = 0.f;
      float wait_ms = 0.f;
      /// seconds the scene, and its scripts, were told had passed for each second of the
      ///  clock. 1 when time is kept; more and everything a script moves is fast
      float engine_seconds_a_second = 0.f;
      float script_seconds_a_second = 0.f;
      float fixed_steps_a_second = 0.f;
    };

    void record(const frame_times& times);

    const summary& latest() const { return shown; }
    /// true on the frame a summary was made, for one who writes each down
    bool summary_is_new() const { return fresh; }
    uint64_t frame_count() const { return frames; }

    /// the lengths of the last frames in milliseconds, oldest first. how many were written
    size_t copy_history(std::span<float> out) const;

   private:
    std::array<float, kHistory> history = {};
    size_t next = 0;
    size_t filled = 0;
    uint64_t frames = 0;

    summary shown;
    bool fresh = false;

    /// what has been added up since the last summary
    frame_times sum;
    double longest = 0.0;
    uint32_t count = 0;
  };

}  // namespace other

#endif  // OTHERLIB_DRIVER_FRAME_STATISTICS_HPP
