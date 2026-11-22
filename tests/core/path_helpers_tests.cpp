/**
 * \file tests/core/path_helpers_tests.cpp
 **/
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "core/defines.hpp"
#include "file/path_helpers.hpp"

namespace other {

  /// root classification runs before any subsystem exists, so the fixture brings none up
  class path_helpers_tests : public ::testing::Test {
   protected:
    void SetUp() override {
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-path-helpers-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      std::filesystem::create_directories(sandbox);
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
    }

    /// lays down the marker directory a packaged build is recognised by
    static filepath make_package(const filepath& exe_dir) {
      std::filesystem::create_directories(exe_dir / "engine" / "resources");
      return exe_dir;
    }

    filepath sandbox;
  };

  TEST_F(path_helpers_tests, packaged_layout_rejects_empty_path) {
    EXPECT_FALSE(is_packaged_layout(filepath{}));
  }

  TEST_F(path_helpers_tests, packaged_layout_rejects_missing_folder) {
    EXPECT_FALSE(is_packaged_layout(sandbox / "does-not-exist"));
  }

  TEST_F(path_helpers_tests, packaged_layout_rejects_folder_without_engine_resources) {
    const filepath plain = sandbox / "plain";
    std::filesystem::create_directories(plain);
    EXPECT_FALSE(is_packaged_layout(plain));

    const filepath engine_only = sandbox / "engine-only";
    std::filesystem::create_directories(engine_only / "engine");
    EXPECT_FALSE(is_packaged_layout(engine_only));
  }

  TEST_F(path_helpers_tests, packaged_layout_accepts_folder_with_engine_resources) {
    EXPECT_TRUE(is_packaged_layout(make_package(sandbox / "My Game")));
  }

  TEST_F(path_helpers_tests, packaged_layout_rejects_engine_resources_file) {
    const filepath exe_dir = sandbox / "file-marker";
    std::filesystem::create_directories(exe_dir / "engine");

    std::ofstream marker(exe_dir / "engine" / "resources");
    ASSERT_TRUE(marker.is_open());
    marker.close();

    EXPECT_FALSE(is_packaged_layout(exe_dir));
  }

  TEST_F(path_helpers_tests, root_for_dev_build_is_parent_of_build_folder) {
    const filepath repo = sandbox / "repo";
    EXPECT_EQ(environment_root_for(repo / "build" / "other-editor" / "Debug"), repo);
  }

  TEST_F(path_helpers_tests, root_for_packaged_build_is_engine_folder) {
    const filepath exe_dir = make_package(sandbox / "My Game");
    EXPECT_EQ(environment_root_for(exe_dir), exe_dir / "engine");
  }

  /// a package compiled into <somewhere>/build/<name> must not classify as a dev build
  TEST_F(path_helpers_tests, root_for_packaged_build_wins_under_build_ancestor) {
    const filepath exe_dir = make_package(sandbox / "build" / "My Game");
    EXPECT_EQ(environment_root_for(exe_dir), exe_dir / "engine");
  }

  TEST_F(path_helpers_tests, root_for_installed_build_is_install_folder) {
    /// relative, so no ancestor is named 'build' wherever the temp folder lives
    const filepath exe_dir = filepath{ "other-sdk" } / "bin" / "release";
    EXPECT_EQ(environment_root_for(exe_dir), get_other_environment_install_folder());
  }

  TEST_F(path_helpers_tests, root_for_empty_exe_dir_is_install_folder) {
    EXPECT_EQ(environment_root_for(filepath{}), get_other_environment_install_folder());
  }

  /// the suite always runs from <repo>/build/tests/<config>
  TEST_F(path_helpers_tests, running_test_binary_classifies_as_dev_build) {
    EXPECT_FALSE(is_packaged_build());
    EXPECT_FALSE(is_installed_build());
    EXPECT_EQ(environment_root(), environment_root_for(get_current_exe_directory()));
  }

}  // namespace other
