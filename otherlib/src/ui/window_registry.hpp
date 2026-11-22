/**
 * \file ui/window_registry.hpp
 **/
#ifndef OTHERLIB_UI_WINDOW_REGISTRY_HPP
#define OTHERLIB_UI_WINDOW_REGISTRY_HPP

#include <span>
#include <string>
#include <string_view>

#include "core/defines.hpp"
#include "core/scope.hpp"
#include "data-structures/std_container.hpp"

#include "ui/ui_window.hpp"

namespace other {
  namespace ui {

    /// who registered a window. a menu of windows lists them a source at a time, in this order
    enum class window_source : uint8_t {
      BUILTIN = 0,
      DRIVER,
      PLUGIN,
      SCRIPT,

      NUM_SOURCES,
    };

    /// every window there is to show, by name: the one list that opening, closing, listing
    ///  and drawing go through, whoever registered the window
    class window_registry {
     public:
      struct entry {
        std::string name;
        natural_t id = 0;
        window_source source = window_source::DRIVER;
        bool open = false;
        scope<ui_window> window = nullptr;
      };

      /// registered closed. the id is the name's hash; 0 when the name is taken or the window
      ///  is null, and the window is then dropped
      natural_t add(const std::string_view name, scope<ui_window> window, window_source source);
      bool remove(natural_t id);
      void clear();

      bool contains(const std::string_view name) const;

      /// false when no window has the name
      bool open(const std::string_view name);
      bool close(const std::string_view name);
      void close_all();

      bool is_open(const std::string_view name) const;
      bool any_open() const;

      /// in the order they were registered
      ostd::vector<std::string> names() const;
      ostd::vector<std::string> open_names() const;
      std::span<const entry> entries() const { return windows; }

      /// draws the open windows. one closed from its own title bar is closed here too
      void render();

     private:
      ostd::vector<entry> windows;

      entry* find(natural_t id);
      const entry* find(natural_t id) const;
    };

  }  // namespace ui
}  // namespace other

#endif  // OTHERLIB_UI_WINDOW_REGISTRY_HPP
