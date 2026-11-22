/**
 * \file tests/core/asset_path_tests.cpp
 *
 * where an asset path leads, and what is said of one that leads nowhere: found quietly, so
 * a caller that asks every frame can report a problem once
 **/
#include <filesystem>
#include <format>
#include <fstream>

#include "file/filesystem.hpp"
#include "file/path_helpers.hpp"

#include "other_test.hpp"

namespace other {

  class asset_path_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-asset-path-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      std::filesystem::create_directories(sandbox);
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
      other_test::TearDown();
    }

    filepath sandbox;
  };

  TEST_F(asset_path_tests, a_path_that_exists_is_found_with_nothing_to_report) {
    const filepath file = sandbox / "texture.png";
    std::ofstream{ file } << "not really a picture";

    const asset_path found = find_asset_path(file.string());
    EXPECT_TRUE(found.found());
    EXPECT_TRUE(found.problem.empty()) << found.problem;
    EXPECT_EQ(found.raw, file.string());
    /// the one that logs leads to the same place
    EXPECT_EQ(resolve_asset_path(file.string()).resolved, found.resolved);
  }

  TEST_F(asset_path_tests, a_path_that_leads_nowhere_says_why) {
    const asset_path absolute = find_asset_path((sandbox / "missing.png").string());
    EXPECT_FALSE(absolute.found());
    EXPECT_TRUE(absolute.problem.starts_with("does not exist")) << absolute.problem;

    const asset_path relative = find_asset_path("no/such/folder/of/this/test/texture.png");
    EXPECT_FALSE(relative.found());
    EXPECT_TRUE(relative.problem.starts_with("not found; searched")) << relative.problem;

    const asset_path unmounted = find_asset_path(std::format("no-such-mount{}texture.png", file_system::kPathSeparator));
    EXPECT_FALSE(unmounted.found());
    EXPECT_TRUE(unmounted.problem.starts_with("names mount 'no-such-mount'")) << unmounted.problem;

    /// the one that logs comes to the same answer
    const asset_path logged = resolve_asset_path("no/such/folder/of/this/test/texture.png");
    EXPECT_FALSE(logged.found());
    EXPECT_EQ(logged.problem, relative.problem);
  }

  TEST_F(asset_path_tests, an_empty_path_is_not_found_and_not_a_problem) {
    const asset_path empty = find_asset_path("");
    EXPECT_FALSE(empty.found());
    EXPECT_TRUE(empty.problem.empty());
  }

}  // namespace other
