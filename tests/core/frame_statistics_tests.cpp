/**
 * \file tests/core/frame_statistics_tests.cpp
 *
 * what the driver's loop says about its frames: the rates and the average frame, how much
 * time the scene and its scripts were told passed, and the history a graph is drawn from
 **/
#include <array>

#include "script/scripting_environment.hpp"

#include "driver/frame_statistics.hpp"
#include "scripting/dotnet_bindings/driver_bindings.hpp"

#include "other_test.hpp"

namespace other {

  class frame_statistics_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    /// frames of one length, enough of them to fill a summary
    static void run(frame_statistics& statistics, double frame_seconds, double script_seconds, uint32_t frames) {
      for (uint32_t i = 0; i < frames; ++i) {
        statistics.record({
          .frame = frame_seconds,
          .update = frame_seconds * 0.25,
          .render = frame_seconds * 0.5,
          .wait = frame_seconds * 0.25,
          .engine = frame_seconds,
          .script = script_seconds,
          .fixed_steps = i % 2,
        });
      }
    }
  };

  TEST_F(frame_statistics_tests, a_summary_is_the_rates_and_the_average_frame) {
    frame_statistics statistics;
    EXPECT_EQ(statistics.latest().frames_a_second, 0.f);

    /// a hundred frames a second for a little over the interval a summary covers
    run(statistics, 0.01, 0.01, 49);
    EXPECT_FALSE(statistics.summary_is_new());
    EXPECT_EQ(statistics.latest().frames_a_second, 0.f);
    run(statistics, 0.01, 0.01, 2);
    EXPECT_EQ(statistics.frame_count(), 51u);

    const frame_statistics::summary& s = statistics.latest();
    EXPECT_NEAR(s.frames_a_second, 100.f, 0.5f);
    EXPECT_NEAR(s.frame_ms, 10.f, 0.01f);
    EXPECT_NEAR(s.longest_frame_ms, 10.f, 0.01f);
    EXPECT_NEAR(s.update_ms, 2.5f, 0.01f);
    EXPECT_NEAR(s.render_ms, 5.f, 0.01f);
    EXPECT_NEAR(s.wait_ms, 2.5f, 0.01f);
    EXPECT_NEAR(s.engine_seconds_a_second, 1.f, 0.001f);
    EXPECT_NEAR(s.script_seconds_a_second, 1.f, 0.001f);
    /// a step every other frame
    EXPECT_NEAR(s.fixed_steps_a_second, 50.f, 2.f);
  }

  TEST_F(frame_statistics_tests, scripts_told_a_sixtieth_at_a_hundred_frames_a_second_run_fast) {
    /// what a driver that never starts a UI frame told its scripts: every frame took a
    ///  sixtieth of a second, however long it took
    frame_statistics statistics;
    run(statistics, 0.01, 1.0 / 60.0, 60);
    EXPECT_NEAR(statistics.latest().engine_seconds_a_second, 1.f, 0.001f);
    EXPECT_NEAR(statistics.latest().script_seconds_a_second, 100.f / 60.f, 0.01f);
  }

  TEST_F(frame_statistics_tests, one_long_frame_is_the_longest_and_the_next_summary_forgets_it) {
    frame_statistics statistics;
    run(statistics, 0.01, 0.01, 20);
    run(statistics, 0.08, 0.08, 1);
    run(statistics, 0.01, 0.01, 30);
    EXPECT_NEAR(statistics.latest().longest_frame_ms, 80.f, 0.01f);
    EXPECT_GT(statistics.latest().frame_ms, 10.f);

    run(statistics, 0.01, 0.01, 60);
    EXPECT_NEAR(statistics.latest().longest_frame_ms, 10.f, 0.01f);
  }

  TEST_F(frame_statistics_tests, the_history_is_the_last_frames_oldest_first) {
    frame_statistics statistics;
    std::array<float, 8> some = {};
    EXPECT_EQ(statistics.copy_history(some), 0u);

    for (uint32_t i = 1; i <= 3; ++i) {
      statistics.record({ .frame = 0.001 * i });
    }
    ASSERT_EQ(statistics.copy_history(some), 3u);
    EXPECT_NEAR(some[0], 1.f, 0.001f);
    EXPECT_NEAR(some[2], 3.f, 0.001f);

    /// more frames than it holds: the newest are kept, and a short span gets the last of them
    for (uint32_t i = 4; i <= frame_statistics::kHistory + 10; ++i) {
      statistics.record({ .frame = 0.001 * i });
    }
    std::array<float, frame_statistics::kHistory> all = {};
    ASSERT_EQ(statistics.copy_history(all), frame_statistics::kHistory);
    EXPECT_NEAR(all.front(), 11.f, 0.001f);
    EXPECT_NEAR(all.back(), static_cast<float>(frame_statistics::kHistory + 10), 0.001f);
    ASSERT_EQ(statistics.copy_history(some), some.size());
    EXPECT_NEAR(some.back(), static_cast<float>(frame_statistics::kHistory + 10), 0.001f);
    EXPECT_NEAR(some.front(), static_cast<float>(frame_statistics::kHistory + 3), 0.001f);
  }

  TEST_F(frame_statistics_tests, a_script_reads_nothing_where_there_is_no_driver) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);
    EXPECT_EQ(env->call_static_dotnet_method<int32_t>("InspectorProbe", "FrameStatsWithNoDriver"), 1);
    EXPECT_EQ(bindings::native_frame_stats_value(0), 0.f);
    EXPECT_EQ(bindings::native_frame_stats_value(900), 0.f);
    float one = 0.f;
    EXPECT_EQ(bindings::native_frame_stats_history(&one, 1), 0);
  }

}  // namespace other
