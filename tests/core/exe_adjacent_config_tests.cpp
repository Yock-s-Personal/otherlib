/**
 * \file tests/core/exe_adjacent_config_tests.cpp
 **/
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "core/defines.hpp"

#include "other.hpp"

namespace other {

  /// the lookup runs before any subsystem exists, so the fixture brings none up
  class exe_adjacent_config_tests : public ::testing::Test {
   protected:
    void SetUp() override {
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-exe-adjacent-config-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      std::filesystem::create_directories(sandbox);
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
    }

    static void touch(const filepath& path) {
      std::ofstream file(path, std::ios::binary);
      ASSERT_TRUE(file.is_open()) << "failed to create " << path.string();
    }

    filepath sandbox;
  };

  TEST_F(exe_adjacent_config_tests, finds_config_beside_exe) {
    touch(sandbox / "other_runtime.exe");
    touch(sandbox / "other_runtime.toml");

    const opt<filepath> found = find_exe_adjacent_config(sandbox / "other_runtime.exe");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, sandbox / "other_runtime.toml");
  }

  TEST_F(exe_adjacent_config_tests, returns_nothing_without_config) {
    touch(sandbox / "other_runtime.exe");
    EXPECT_FALSE(find_exe_adjacent_config(sandbox / "other_runtime.exe").has_value());
  }

  TEST_F(exe_adjacent_config_tests, returns_nothing_for_empty_exe_path) {
    EXPECT_FALSE(find_exe_adjacent_config(filepath{}).has_value());
  }

  /// compile renames the runtime to the public name, which may contain spaces
  TEST_F(exe_adjacent_config_tests, keeps_spaces_in_exe_name) {
    touch(sandbox / "My Game.exe");
    touch(sandbox / "My Game.toml");

    const opt<filepath> found = find_exe_adjacent_config(sandbox / "My Game.exe");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, sandbox / "My Game.toml");
  }

  TEST_F(exe_adjacent_config_tests, strips_only_the_last_extension) {
    touch(sandbox / "a.b.exe");
    touch(sandbox / "a.b.toml");
    touch(sandbox / "a.toml");

    const opt<filepath> found = find_exe_adjacent_config(sandbox / "a.b.exe");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, sandbox / "a.b.toml");
  }

  TEST_F(exe_adjacent_config_tests, ignores_config_of_shorter_stem) {
    touch(sandbox / "a.b.exe");
    touch(sandbox / "a.toml");
    EXPECT_FALSE(find_exe_adjacent_config(sandbox / "a.b.exe").has_value());
  }

  TEST_F(exe_adjacent_config_tests, ignores_directory_named_like_config) {
    touch(sandbox / "game.exe");
    std::filesystem::create_directories(sandbox / "game.toml");
    EXPECT_FALSE(find_exe_adjacent_config(sandbox / "game.exe").has_value());
  }

  TEST_F(exe_adjacent_config_tests, ignores_config_outside_exe_folder) {
    std::filesystem::create_directories(sandbox / "bin");
    touch(sandbox / "bin" / "game.exe");
    touch(sandbox / "game.toml");
    EXPECT_FALSE(find_exe_adjacent_config(sandbox / "bin" / "game.exe").has_value());
  }

}  // namespace other
