/**
 * \file driver/frame_statistics.cpp
 **/
#include "driver/frame_statistics.hpp"

#include <algorithm>

namespace other {

  void frame_statistics::record(const frame_times& times) {
    ++frames;
    history[next] = static_cast<float>(times.frame * 1000.0);
    next = (next + 1) % kHistory;
    filled = std::min(filled + 1, kHistory);

    sum.frame += times.frame;
    sum.update += times.update;
    sum.render += times.render;
    sum.wait += times.wait;
    sum.engine += times.engine;
    sum.script += times.script;
    sum.fixed_steps += times.fixed_steps;
    longest = std::max(longest, times.frame);
    ++count;

    fresh = false;
    if (sum.frame < kSummaryInterval) {
      return;
    }

    const double each = 1000.0 / static_cast<double>(count);
    shown = {
      .frames_a_second = static_cast<float>(static_cast<double>(count) / sum.frame),
      .frame_ms = static_cast<float>(sum.frame * each),
      .longest_frame_ms = static_cast<float>(longest * 1000.0),
      .update_ms = static_cast<float>(sum.update * each),
      .render_ms = static_cast<float>(sum.render * each),
      .wait_ms = static_cast<float>(sum.wait * each),
      .engine_seconds_a_second = static_cast<float>(sum.engine / sum.frame),
      .script_seconds_a_second = static_cast<float>(sum.script / sum.frame),
      .fixed_steps_a_second = static_cast<float>(static_cast<double>(sum.fixed_steps) / sum.frame),
    };
    fresh = true;
    sum = {};
    longest = 0.0;
    count = 0;
  }

  size_t frame_statistics::copy_history(std::span<float> out) const {
    const size_t wanted = std::min(out.size(), filled);
    /// the newest is just before next; the oldest of what is wanted is that many back
    size_t at = (next + kHistory - wanted) % kHistory;
    for (size_t i = 0; i < wanted; ++i) {
      out[i] = history[at];
      at = (at + 1) % kHistory;
    }
    return wanted;
  }

}  // namespace other
