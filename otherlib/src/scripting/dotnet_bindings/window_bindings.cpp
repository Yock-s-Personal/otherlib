/**
 * \file scripting/dotnet_bindings/window_bindings.cpp
 **/
#include "scripting/dotnet_bindings/window_bindings.hpp"

#include <string_view>

#include <glm/glm.hpp>

#include "core/profiler.hpp"
#include "event/event_system.hpp"

#include "script/scripting_environment.hpp"

#include "ui/driver_ui.hpp"
#include "ui/script_window.hpp"
#include "ui/window_registry.hpp"

namespace other {
  namespace bindings {

    namespace {

      constexpr std::string_view kWindowHost = "Other.UI.WindowHost";

      struct window_bridge {
        ui::window_registry* registry = nullptr;
        event_system* events = nullptr;
        driver_ui* menus = nullptr;
        void (*menu_item_clicked)(natural_t item_id) = nullptr;
      };

      window_bridge& bridge() {
        static window_bridge state;
        return state;
      }

      template <typename... Args>
      void call_window_host(const std::string_view method, Args... args) {
        if (subsystem<scripting_environment>::inert) {
          return;
        }
        auto* env = subsystem<scripting_environment>::get();
        if (env == nullptr) {
          return;
        }
        env->call_static_dotnet_method<void>(kWindowHost, method, args...);
      }

      const ui::script_window* find_script_window(const char* name) {
        if (bridge().registry == nullptr) {
          return nullptr;
        }

        const natural_t id = FNV(name);
        for (const auto& window : bridge().registry->entries()) {
          if (window.id == id) {
            return dynamic_cast<const ui::script_window*>(window.window.get());
          }
        }
        return nullptr;
      }

    }  // namespace

    natural_t native_ui_register_window(const char* name, const char* title, float width, float height, int32_t flags) {
      PROFILE_SECTION("bindings::native_ui_register_window");
      window_bridge& state = bridge();
      if (state.registry == nullptr) {
        return 0;
      }

      auto window = make_scope<ui::script_window>(state.events, name, title, FNV(name), glm::vec2{ width, height }, flags);
      return state.registry->add(name, std::move(window), ui::window_source::SCRIPT);
    }

    void native_ui_unregister_window(natural_t id) {
      if (bridge().registry != nullptr) {
        bridge().registry->remove(id);
      }
    }

    void native_ui_set_window_draw_function(void* fn) {
      ui::script_window::set_draw_function(reinterpret_cast<ui::script_window::draw_fn>(fn));
    }

    nbool32 native_ui_open_window(const char* name) {
      return bridge().registry != nullptr && bridge().registry->open(name);
    }

    nbool32 native_ui_close_window(const char* name) {
      return bridge().registry != nullptr && bridge().registry->close(name);
    }

    nbool32 native_ui_is_window_open(const char* name) {
      return bridge().registry != nullptr && bridge().registry->is_open(name);
    }

    natural_t native_ui_window_draw_count(const char* name) {
      const ui::script_window* window = find_script_window(name);
      return window != nullptr ? window->draw_count() : 0;
    }

    float native_ui_window_draw_microseconds(const char* name) {
      const ui::script_window* window = find_script_window(name);
      return window != nullptr ? window->last_draw_microseconds() : 0.f;
    }

    natural_t native_ui_add_menu_item(const char* path, const char* label) {
      if (bridge().menus == nullptr) {
        return 0;
      }

      return bridge().menus->add_menu_item(path, label, [](natural_t id) {
        if (bridge().menu_item_clicked != nullptr) {
          bridge().menu_item_clicked(id);
        }
      });
    }

    void native_ui_remove_menu_item(natural_t id) {
      if (bridge().menus != nullptr) {
        bridge().menus->remove_menu_item(id);
      }
    }

    void native_ui_set_menu_item_shortcut(natural_t id, const char* shortcut) {
      if (bridge().menus != nullptr) {
        bridge().menus->set_menu_item_shortcut(id, shortcut != nullptr ? std::string_view{ shortcut } : std::string_view{});
      }
    }

    void native_ui_set_menu_item_enabled(natural_t id, nbool32 enabled) {
      if (bridge().menus != nullptr) {
        bridge().menus->set_menu_item_enabled(id, enabled);
      }
    }

    void native_ui_set_menu_item_click_function(void* fn) {
      bridge().menu_item_clicked = reinterpret_cast<void (*)(natural_t)>(fn);
    }

    void attach_script_windows(ui::window_registry* registry, event_system* events, driver_ui* menus) {
      PROFILE_SECTION("bindings::attach_script_windows");
      OTHER_ASSERT(registry != nullptr, "There is no window registry to attach script windows to.");
      OTHER_ASSERT(events != nullptr, "There is no event system to make script windows with.");
      OTHER_ASSERT(bridge().registry == nullptr, "Script windows are already attached to a window registry.");
      bridge().registry = registry;
      bridge().events = events;
      bridge().menus = menus;
      call_window_host("Attach");
    }

    void detach_script_windows() {
      PROFILE_SECTION("bindings::detach_script_windows");
      if (bridge().registry == nullptr) {
        return;
      }

      call_window_host("Detach");
      ui::script_window::set_draw_function(nullptr);
      bridge().registry = nullptr;
      bridge().events = nullptr;
      bridge().menus = nullptr;
      bridge().menu_item_clicked = nullptr;
    }

    void host_script_windows_of(int32_t assembly_id) {
      PROFILE_SECTION("bindings::host_script_windows_of");
      call_window_host("AssemblyLoaded", assembly_id);
    }

  }  // namespace bindings
}  // namespace other
