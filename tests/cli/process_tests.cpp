/**
 * \file tests/cli/process_tests.cpp
 *
 * a child process that is waited for: one that ends by itself, and one that is told to
 * stop and takes the processes it started with it
 **/
#include <chrono>
#include <filesystem>
#include <thread>

#include <gtest/gtest.h>

#include "cli/process.hpp"
#include "other_test.hpp"

namespace other {

  class process_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      sandbox = std::filesystem::temp_directory_path() / "other-process-tests" / ::testing::UnitTest::GetInstance()->current_test_info()->name();
      std::filesystem::remove_all(sandbox);
      std::filesystem::create_directories(sandbox);
      shell = cli::find_program_on_path("cmd");
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
      other_test::TearDown();
    }

    filepath sandbox;
    opt<filepath> shell;
  };

  TEST_F(process_tests, a_child_that_ends_by_itself_was_not_stopped) {
    ASSERT_TRUE(shell.has_value());
    uint32_t asked = 0;
    const cli::process_result result = cli::launch_process({
      .executable = *shell,
      .arguments = { "/c", "exit 3" },
      .working_directory = sandbox,
      .wait_for_exit = true,
      .new_console = false,
      .should_stop = [&asked]() -> bool {
        ++asked;
        return false;
      },
    });
    EXPECT_TRUE(result.started) << result.error;
    EXPECT_FALSE(result.stopped);
    EXPECT_EQ(result.exit_code, 3);
  }

  TEST_F(process_tests, a_child_told_to_stop_ends_with_every_process_it_started) {
    ASSERT_TRUE(shell.has_value());
    /// the shell starts a program that would run for a minute and holds held.txt open
    uint32_t asked = 0;
    const auto began = std::chrono::steady_clock::now();
    const cli::process_result result = cli::launch_process({
      .executable = *shell,
      .arguments = { "/c", "ping -n 60 127.0.0.1 >held.txt" },
      .working_directory = sandbox,
      .wait_for_exit = true,
      .new_console = false,
      .should_stop = [&asked]() -> bool { return ++asked >= 5; },
    });
    const auto took = std::chrono::steady_clock::now() - began;

    EXPECT_TRUE(result.started) << result.error;
    EXPECT_TRUE(result.stopped);
    EXPECT_EQ(asked, 5u);
    EXPECT_LT(took, std::chrono::seconds(20));

    /// the file can be removed once nothing holds it: the shell and what it started are gone
    const filepath held = sandbox / "held.txt";
    ASSERT_TRUE(std::filesystem::exists(held));
    bool removed = false;
    for (uint32_t attempt = 0; attempt < 50 && !removed; ++attempt) {
      std::error_code ec;
      removed = std::filesystem::remove(held, ec);
      if (!removed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
    EXPECT_TRUE(removed) << "something the stopped child started still holds its output file";
  }

}  // namespace other
