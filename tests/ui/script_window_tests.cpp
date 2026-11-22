/**
 * \file tests/ui/script_window_tests.cpp
 *
 * a window declared in C# is a window in the registry like any other, drawn by its assembly
 * only while it is open and shown, and gone and back with that assembly
 **/
#include <asio/asio.hpp>

#include "event/event_system.hpp"

#include "script/scripting_environment.hpp"

#include "scripting/dotnet_bindings/window_bindings.hpp"
#include "ui/script_window.hpp"
#include "ui/window_registry.hpp"

#include "headless_imgui.hpp"
#include "other_test.hpp"

namespace other {
  namespace {

    constexpr int32_t kCounter = 0;
    constexpr int32_t kThrower = 1;

    const ui::window_registry::entry* entry_of(const ui::window_registry& registry, const std::string_view name) {
      for (const auto& window : registry.entries()) {
        if (window.name == name) {
          return &window;
        }
      }
      return nullptr;
    }

  }  // namespace

  class script_window_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    void SetUp() override {
      other_test::SetUp();
      env = subsystem<scripting_environment>::get();
      ASSERT_NE(env, nullptr);

      registry = make_scope<ui::window_registry>();
      bindings::attach_script_windows(registry.get(), &events);
      host_testing_assembly();
    }

    void TearDown() override {
      bindings::detach_script_windows();
      registry = nullptr;
      other_test::TearDown();
    }

    /// what a driver does for each of its own modules: a project's assembly is not asked
    void host_testing_assembly() {
      ref<assembly> testing = env->get_dotnet_module_by_asset_path(testing_dll);
      ASSERT_NE(testing, nullptr);
      bindings::host_script_windows_of(testing->dotnet_id);
    }

    int32_t probe(const std::string_view method) {
      return env->call_static_dotnet_method<int32_t>("WindowProbe", method);
    }

    int32_t probe(const std::string_view method, int32_t which) {
      return env->call_static_dotnet_method<int32_t>("WindowProbe", method, which);
    }

    ui::script_window* script_window(const std::string_view name) {
      const auto* found = entry_of(*registry, name);
      return found != nullptr ? dynamic_cast<ui::script_window*>(found->window.get()) : nullptr;
    }

    void frames(int32_t count) {
      for (int32_t i = 0; i < count; ++i) {
        imgui.frame([this]() { registry->render(); });
      }
    }

    const filepath testing_dll = perform_tag_replacement("build/script-testing/${build-config}/DotnetTesting.dll");

    scripting_environment* env = nullptr;
    asio::io_context io_context;
    event_system events{ io_context };
    headless_imgui imgui;
    scope<ui::window_registry> registry = nullptr;
  };

  TEST_F(script_window_tests, a_class_declared_a_window_is_in_the_registry) {
    const auto* counter = entry_of(*registry, "test-counter");
    ASSERT_NE(counter, nullptr);
    EXPECT_EQ(counter->source, ui::window_source::SCRIPT);
    EXPECT_EQ(counter->window->title, "Test Counter");
    EXPECT_FALSE(counter->open);
    EXPECT_NE(dynamic_cast<const ui::script_window*>(counter->window.get()), nullptr);

    /// no title declared: the name is the title
    const auto* thrower = entry_of(*registry, "test-thrower");
    ASSERT_NE(thrower, nullptr);
    EXPECT_EQ(thrower->window->title, "test-thrower");

    /// declared a window on a class that is not one
    EXPECT_FALSE(registry->contains("test-not-a-window"));
    EXPECT_EQ(registry->entries().size(), 4u);
    EXPECT_EQ(probe("CounterInitialized"), 1);
  }

  TEST_F(script_window_tests, a_closed_window_makes_no_call_into_its_script) {
    frames(5);
    EXPECT_EQ(probe("CounterDraws"), 0);
    EXPECT_EQ(bindings::native_ui_window_draw_count("test-counter"), 0u);
  }

  TEST_F(script_window_tests, an_open_window_is_drawn_once_a_frame) {
    ASSERT_TRUE(registry->open("test-counter"));
    frames(4);
    EXPECT_EQ(probe("CounterDraws"), 4);
    EXPECT_EQ(bindings::native_ui_window_draw_count("test-counter"), 4u);

    registry->close("test-counter");
    frames(3);
    EXPECT_EQ(probe("CounterDraws"), 4);
  }

  TEST_F(script_window_tests, a_window_that_throws_is_stopped_and_the_others_go_on) {
    ASSERT_TRUE(registry->open("test-thrower"));
    ASSERT_TRUE(registry->open("test-counter"));
    frames(5);
    /// asked once: the id it left pushed is popped for it, and it is not asked again
    EXPECT_EQ(probe("ThrowerDraws"), 1);
    EXPECT_EQ(probe("CounterDraws"), 5);

    /// it is still a window, open, saying why it is blank
    ui::script_window* thrower = script_window("test-thrower");
    ASSERT_NE(thrower, nullptr);
    EXPECT_TRUE(thrower->is_disabled());
    EXPECT_FALSE(thrower->disabled_reason().empty());
    EXPECT_TRUE(registry->is_open("test-thrower"));
    EXPECT_FALSE(script_window("test-counter")->is_disabled());
  }

  TEST_F(script_window_tests, a_window_imgui_objects_to_is_stopped_without_having_thrown) {
    ASSERT_TRUE(registry->open("test-unbalanced"));
    ASSERT_TRUE(registry->open("test-overpop"));
    ASSERT_TRUE(registry->open("test-counter"));
    frames(5);
    EXPECT_EQ(probe("UnbalancedDraws"), 1);
    EXPECT_EQ(probe("OverpopDraws"), 1);
    EXPECT_EQ(probe("CounterDraws"), 5);

    /// the reason is what ImGui said, which names the call
    ui::script_window* unbalanced = script_window("test-unbalanced");
    ui::script_window* overpop = script_window("test-overpop");
    ASSERT_NE(unbalanced, nullptr);
    ASSERT_NE(overpop, nullptr);
    EXPECT_TRUE(unbalanced->is_disabled());
    EXPECT_NE(unbalanced->disabled_reason().find("PopID"), std::string::npos) << unbalanced->disabled_reason();
    EXPECT_TRUE(overpop->is_disabled());
    EXPECT_NE(overpop->disabled_reason().find("PopID"), std::string::npos) << overpop->disabled_reason();
  }

  TEST_F(script_window_tests, a_stopped_window_asked_again_is_drawn_once_more) {
    ASSERT_TRUE(registry->open("test-thrower"));
    frames(3);
    ui::script_window* thrower = script_window("test-thrower");
    ASSERT_NE(thrower, nullptr);
    ASSERT_TRUE(thrower->is_disabled());

    /// what the button in the stopped window does. the bug is still there
    thrower->enable();
    EXPECT_FALSE(thrower->is_disabled());
    frames(3);
    EXPECT_EQ(probe("ThrowerDraws"), 2);
    EXPECT_TRUE(thrower->is_disabled());
  }

  TEST_F(script_window_tests, a_script_opens_and_closes_windows_by_name) {
    EXPECT_EQ(probe("IsOpen", kCounter), 0);
    EXPECT_EQ(probe("Open", kCounter), 1);
    EXPECT_TRUE(registry->is_open("test-counter"));
    EXPECT_EQ(probe("IsOpen", kCounter), 1);
    EXPECT_EQ(probe("IsOpen", kThrower), 0);

    EXPECT_EQ(probe("Close", kCounter), 1);
    EXPECT_FALSE(registry->is_open("test-counter"));
    EXPECT_EQ(probe("OpenMissing"), 0);
  }

  TEST_F(script_window_tests, detaching_shuts_the_windows_down_and_attaching_brings_them_back) {
    bindings::detach_script_windows();
    EXPECT_TRUE(registry->entries().empty());
    EXPECT_EQ(probe("CounterShutDown"), 1);
    /// with nowhere to open a window, opening one says so
    EXPECT_EQ(probe("Open", kCounter), 0);

    bindings::attach_script_windows(registry.get(), &events);
    EXPECT_TRUE(registry->contains("test-counter"));
    EXPECT_EQ(probe("CounterInitialized"), 2);
  }

  TEST_F(script_window_tests, windows_go_with_their_assembly_and_come_back_as_they_were) {
    ASSERT_TRUE(registry->open("test-counter"));
    frames(2);
    ASSERT_EQ(probe("CounterDraws"), 2);

    env->unload_dotnet_module(env->get_dotnet_module_by_asset_path(testing_dll));
    EXPECT_TRUE(registry->entries().empty());
    /// nothing is left to call into the assembly that went
    frames(2);

    ASSERT_NE(env->load_dotnet_module(testing_dll.string()), nullptr);
    host_testing_assembly();
    EXPECT_TRUE(registry->contains("test-thrower"));
    EXPECT_FALSE(registry->is_open("test-thrower"));
    /// the one that was open is open, and drawn by the assembly that was just loaded
    EXPECT_TRUE(registry->is_open("test-counter"));
    frames(3);
    EXPECT_EQ(probe("CounterDraws"), 3);
  }

}  // namespace other
