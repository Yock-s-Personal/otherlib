/**
 * \file tests/scripting/main_thread_only_tests.cpp
 *
 * what a script is given in place of a native function: the function itself on the main
 * thread, and a refused call with one error from any other
 **/
#include <thread>

#include "thread/thread_safety.hpp"

#include "scripting/main_thread_only.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    int32_t calls = 0;

    int32_t add_one(int32_t value) {
      ++calls;
      return value + 1;
    }

    void touch() {
      ++calls;
    }

  }  // namespace

  class main_thread_only_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      calls = 0;
      /// the suite runs with the thread check off; these are about the check
      register_main_thread();
      enable_thread_check();
    }

    void TearDown() override {
      disable_thread_check();
      other_test::TearDown();
    }
  };

  TEST_F(main_thread_only_tests, a_call_from_the_main_thread_is_the_function_itself) {
    EXPECT_EQ(bindings::main_thread_only<add_one>::call(4), 5);
    bindings::main_thread_only<touch>::call();
    EXPECT_EQ(calls, 2);
  }

  TEST_F(main_thread_only_tests, a_call_from_another_thread_is_not_made_and_is_reported) {
    bindings::main_thread_only<add_one>::name = "AddOne";
    bindings::main_thread_only<touch>::name = "Touch";

    int32_t returned = -1;
    std::thread other_thread([&returned]() {
      returned = bindings::main_thread_only<add_one>::call(4);
      bindings::main_thread_only<add_one>::call(4);
      bindings::main_thread_only<touch>::call();
    });
    other_thread.join();

    /// an empty value, and the functions never ran
    EXPECT_EQ(returned, 0);
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(bindings::main_thread_only<add_one>::reported.load());
    EXPECT_TRUE(bindings::main_thread_only<touch>::reported.load());

    /// the main thread is not shut out by what another thread did
    EXPECT_EQ(bindings::main_thread_only<add_one>::call(4), 5);
    EXPECT_EQ(calls, 1);
  }

}  // namespace other
