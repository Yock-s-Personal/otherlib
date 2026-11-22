/**
 * \file ui/window_registry.cpp
 **/
#include "ui/window_registry.hpp"

#include <algorithm>

#include "core/logger.hpp"
#include "core/profiler.hpp"

namespace other {
  namespace ui {

    natural_t window_registry::add(const std::string_view name, scope<ui_window> window, window_source source) {
      PROFILE_SECTION("window_registry::add");
      if (window == nullptr) {
        CORE_LOG_ERROR("The window '{}' was not registered: there is no window.", name);
        return 0;
      }

      const natural_t id = FNV(name);
      if (find(id) != nullptr) {
        CORE_LOG_ERROR("The window '{}' was not registered: a window with that name already is.", name);
        return 0;
      }

      windows.push_back(entry{
        .name = std::string{ name },
        .id = id,
        .source = source,
        .open = false,
        .window = std::move(window),
      });
      CORE_LOG_DEBUG("Registered window: {} [{}]", name, id);
      return id;
    }

    bool window_registry::remove(natural_t id) {
      PROFILE_SECTION("window_registry::remove");
      const auto itr = std::ranges::find(windows, id, &entry::id);
      if (itr == windows.end()) {
        return false;
      }

      CORE_LOG_DEBUG("Unregistered window: {} [{}]", itr->name, id);
      windows.erase(itr);
      return true;
    }

    void window_registry::clear() {
      windows.clear();
    }

    bool window_registry::contains(const std::string_view name) const {
      return find(FNV(name)) != nullptr;
    }

    bool window_registry::open(const std::string_view name) {
      entry* found = find(FNV(name));
      if (found == nullptr) {
        return false;
      }

      found->window->toggle_open();
      found->open = true;
      return true;
    }

    bool window_registry::close(const std::string_view name) {
      entry* found = find(FNV(name));
      if (found == nullptr) {
        return false;
      }

      found->window->toggle_close();
      found->open = false;
      return true;
    }

    void window_registry::close_all() {
      for (entry& window : windows) {
        if (window.open) {
          window.window->toggle_close();
          window.open = false;
        }
      }
    }

    bool window_registry::is_open(const std::string_view name) const {
      const entry* found = find(FNV(name));
      return found != nullptr && found->open;
    }

    bool window_registry::any_open() const {
      return std::ranges::any_of(windows, &entry::open);
    }

    ostd::vector<std::string> window_registry::names() const {
      ostd::vector<std::string> result;
      result.reserve(windows.size());
      for (const entry& window : windows) {
        result.push_back(window.name);
      }
      return result;
    }

    ostd::vector<std::string> window_registry::open_names() const {
      ostd::vector<std::string> result;
      for (const entry& window : windows) {
        if (window.open) {
          result.push_back(window.name);
        }
      }
      return result;
    }

    void window_registry::render() {
      PROFILE_SECTION("window_registry::render");
      /// by index: a window's body may open or close another, which never moves an entry
      for (size_t i = 0; i < windows.size(); ++i) {
        entry& window = windows[i];
        if (!window.open) {
          continue;
        }

        window.window->render();
        window.open = !window.window->just_closed();
      }
    }

    window_registry::entry* window_registry::find(natural_t id) {
      const auto itr = std::ranges::find(windows, id, &entry::id);
      return itr == windows.end() ? nullptr : &*itr;
    }

    const window_registry::entry* window_registry::find(natural_t id) const {
      const auto itr = std::ranges::find(windows, id, &entry::id);
      return itr == windows.end() ? nullptr : &*itr;
    }

  }  // namespace ui
}  // namespace other
