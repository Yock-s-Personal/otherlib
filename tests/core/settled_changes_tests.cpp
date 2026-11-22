/**
 * \file tests/core/settled_changes_tests.cpp
 *
 * a changed file is held back until it has stopped changing: one that is missing, still
 * being written or changed again is not handed on, and one that never comes back is given up
 **/
#include <filesystem>
#include <fstream>

#include "file/settled_changes.hpp"

#include "other_test.hpp"

namespace other {

  class settled_changes_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-settled-changes-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      std::filesystem::create_directories(sandbox);
      file = sandbox / "Game.dll";
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
      other_test::TearDown();
    }

    void write(const std::string_view text) const {
      std::ofstream out(file, std::ios::binary | std::ios::trunc);
      out.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    filepath sandbox;
    filepath file;
  };

  TEST_F(settled_changes_tests, a_changed_file_is_handed_on_once_it_is_the_same_at_two_checks_in_a_row) {
    settled_changes changes;
    EXPECT_TRUE(changes.empty());
    write("first");
    changes.note(file);
    EXPECT_FALSE(changes.empty());

    /// seen for the first time: nothing to compare it with yet
    EXPECT_TRUE(changes.check().settled.empty());
    const settled_changes::result second = changes.check();
    ASSERT_EQ(second.settled.size(), 1);
    EXPECT_EQ(second.settled.front(), file);
    EXPECT_TRUE(second.gone.empty());
    /// and it is handed on once
    EXPECT_TRUE(changes.empty());
    EXPECT_TRUE(changes.check().settled.empty());
  }

  TEST_F(settled_changes_tests, a_file_that_is_being_replaced_waits_until_it_is_back_and_whole) {
    settled_changes changes;
    /// a build removes its output before it writes the new one
    changes.note(file);
    EXPECT_TRUE(changes.check().settled.empty());
    EXPECT_TRUE(changes.check().settled.empty());
    EXPECT_FALSE(changes.empty());

    write("half");
    EXPECT_TRUE(changes.check().settled.empty());
    /// written again between two checks: it has not stopped changing
    write("the whole assembly");
    EXPECT_TRUE(changes.check().settled.empty());
    EXPECT_EQ(changes.check().settled.size(), 1);
  }

  TEST_F(settled_changes_tests, a_file_noted_again_starts_over) {
    settled_changes changes;
    write("first");
    changes.note(file);
    EXPECT_TRUE(changes.check().settled.empty());
    /// another change is reported before the first has settled
    changes.note(file);
    EXPECT_TRUE(changes.check().settled.empty());
    EXPECT_EQ(changes.check().settled.size(), 1);
  }

  TEST_F(settled_changes_tests, a_file_that_stays_away_is_given_up_and_each_file_is_judged_alone) {
    settled_changes changes;
    const filepath removed = sandbox / "Removed.dll";
    write("here");
    changes.note(file);
    changes.note(removed);

    size_t settled = 0;
    for (uint32_t check = 1; check < settled_changes::kChecksUntilGone; ++check) {
      const settled_changes::result result = changes.check();
      settled += result.settled.size();
      EXPECT_TRUE(result.gone.empty()) << "check " << check;
    }
    EXPECT_EQ(settled, 1);

    const settled_changes::result last = changes.check();
    ASSERT_EQ(last.gone.size(), 1);
    EXPECT_EQ(last.gone.front(), removed);
    EXPECT_TRUE(changes.empty());
  }

}  // namespace other
