/**
 * \file tests/ui/window_registry_tests.cpp
 *
 * the one list of windows: registering, opening, closing and listing, with no driver
 **/
#include <asio/asio.hpp>

#include "event/event_system.hpp"

#include "ui/window_registry.hpp"

#include "headless_imgui.hpp"
#include "other_test.hpp"

namespace other {
  namespace {

    class counting_window : public ui_window {
     public:
      counting_window(event_system* events, const std::string_view title, int32_t* bodies)
          : ui_window(events, title), bodies(bodies) {}

     protected:
      void on_render_body() override {
        ++*bodies;
      }

     private:
      int32_t* bodies = nullptr;
    };

  }  // namespace

  class window_registry_tests : public other_test {
   protected:
    scope<ui_window> window(const std::string_view title) {
      return make_scope<counting_window>(&events, title, &bodies);
    }

    asio::io_context io_context;
    event_system events{ io_context };
    int32_t bodies = 0;
  };

  TEST_F(window_registry_tests, a_window_is_registered_closed_under_its_name) {
    ui::window_registry registry;
    const natural_t id = registry.add("first", window("First"), ui::window_source::DRIVER);
    EXPECT_EQ(id, FNV("first"));
    EXPECT_TRUE(registry.contains("first"));
    EXPECT_FALSE(registry.is_open("first"));
    EXPECT_FALSE(registry.any_open());
    EXPECT_FALSE(registry.contains("second"));
  }

  TEST_F(window_registry_tests, a_name_is_registered_once) {
    ui::window_registry registry;
    ASSERT_NE(registry.add("first", window("First"), ui::window_source::DRIVER), 0u);
    EXPECT_EQ(registry.add("first", window("Another"), ui::window_source::SCRIPT), 0u);
    EXPECT_EQ(registry.add("null", nullptr, ui::window_source::DRIVER), 0u);
    ASSERT_EQ(registry.entries().size(), 1u);
    EXPECT_EQ(registry.entries()[0].window->title, "First");
  }

  TEST_F(window_registry_tests, opening_and_closing_go_by_name_whoever_registered_the_window) {
    ui::window_registry registry;
    registry.add("builtin", window("Builtin"), ui::window_source::BUILTIN);
    registry.add("driver", window("Driver"), ui::window_source::DRIVER);
    registry.add("plugin", window("Plugin"), ui::window_source::PLUGIN);
    registry.add("script", window("Script"), ui::window_source::SCRIPT);

    for (const std::string_view name : { "builtin", "driver", "plugin", "script" }) {
      EXPECT_TRUE(registry.open(name));
      EXPECT_TRUE(registry.is_open(name));
    }
    EXPECT_FALSE(registry.open("nobody"));
    EXPECT_FALSE(registry.close("nobody"));

    EXPECT_TRUE(registry.close("plugin"));
    EXPECT_FALSE(registry.is_open("plugin"));
    EXPECT_TRUE(registry.any_open());

    const ostd::vector<std::string> open = registry.open_names();
    ASSERT_EQ(open.size(), 3u);
    EXPECT_EQ(open[0], "builtin");
    EXPECT_EQ(open[1], "driver");
    EXPECT_EQ(open[2], "script");

    /// every name, in the order registered
    const ostd::vector<std::string> names = registry.names();
    ASSERT_EQ(names.size(), 4u);
    EXPECT_EQ(names[2], "plugin");

    registry.close_all();
    EXPECT_FALSE(registry.any_open());
    EXPECT_TRUE(registry.open_names().empty());
  }

  TEST_F(window_registry_tests, a_removed_window_is_gone_and_its_name_is_free) {
    ui::window_registry registry;
    const natural_t id = registry.add("first", window("First"), ui::window_source::DRIVER);
    registry.open("first");

    EXPECT_TRUE(registry.remove(id));
    EXPECT_FALSE(registry.remove(id));
    EXPECT_FALSE(registry.contains("first"));
    EXPECT_FALSE(registry.any_open());
    EXPECT_NE(registry.add("first", window("First"), ui::window_source::DRIVER), 0u);
  }

  TEST_F(window_registry_tests, only_open_windows_are_drawn) {
    headless_imgui imgui;
    ui::window_registry registry;
    registry.add("shown", window("Shown"), ui::window_source::DRIVER);
    registry.add("hidden", window("Hidden"), ui::window_source::DRIVER);

    imgui.frame([&]() { registry.render(); });
    EXPECT_EQ(bodies, 0);

    registry.open("shown");
    imgui.frame([&]() { registry.render(); });
    imgui.frame([&]() { registry.render(); });
    EXPECT_EQ(bodies, 2);

    registry.close("shown");
    imgui.frame([&]() { registry.render(); });
    EXPECT_EQ(bodies, 2);
  }

}  // namespace other
