/**
 * \file ui/driver_ui.hpp
 **/
#ifndef OTHERLIB_UI_DRIVER_UI_HPP
#define OTHERLIB_UI_DRIVER_UI_HPP

#include <cstdint>
#include <functional>
#include <string>

#include "ui/menu-bar/menu_bar.hpp"
#include "ui/ui_window.hpp"
#include "ui/window_registry.hpp"

namespace other {

  class driver;
  class dotnet_object;

  class driver_ui {
   public:
    driver_ui(driver* drv)
        : driver_ptr(drv) {}
    virtual ~driver_ui() = default;

    void initialize();
    void render();
    void shutdown();

    void available_ui_window_menu();

    ostd::vector<std::string> get_open_window_names() const;
    ostd::vector<std::string> get_available_window_names() const;

    void open_window(const std::string_view window_name);
    void close_window(const std::string_view window_name);
    void close_all_windows();
    bool is_window_open(const std::string_view window_name) const;
    bool any_window_open() const;

    void register_main_menu_bar_menu(const std::string_view menu_name);
    void register_main_menu_bar_menu(const ui::menu& menu);
    void register_main_menu_bar_sub_menu(const std::string_view menu_name, const ui::menu& sub_menu);
    void register_main_menu_bar_menu_item(const std::string_view menu_name, const ui::menu_item& item);

    /// path names the menu and any menus inside it: "File/Project". what comes back takes
    ///  the item out again; 0 when the path names no menu
    natural_t add_menu_item(const std::string_view path, const std::string_view label, std::function<void(natural_t)> on_click);
    void remove_menu_item(natural_t id);
    /// for an item add_menu_item made: the text at its right edge, and whether it can be clicked
    void set_menu_item_shortcut(natural_t id, const std::string_view shortcut);
    void set_menu_item_enabled(natural_t id, bool enabled);

    /// 0 when the name is taken
    natural_t register_window(const std::string_view name, scope<ui_window> window, ui::window_source source = ui::window_source::DRIVER);
    template <typename T, typename... Args>
      requires std::derived_from<T, ui_window>
    natural_t register_window(const std::string_view name, Args&&... args) {
      return register_window(name, make_scope<T>(std::forward<Args>(args)...));
    }

    void unregister_window(natural_t id);
    void unregister_window(const std::string_view name);

   private:
    struct main_menu_bar_menu {
      natural_t hash;
      ui::menu menu;
    };

    bool main_menu_bar_open = false;
    /// the key under Esc opens and shuts the console (ui.console-key): how a packaged game
    ///  with no menu bar reaches it
    bool console_key = false;
    driver* driver_ptr = nullptr;
    ui::window_registry windows;

    ui::menu_bar main_menu_bar;
    ostd::vector<main_menu_bar_menu> main_menu_bar_menus;
    natural_t last_menu_item_id = 0;

    event_system& events();

    /// a driver whose config names a theme draws with it; one that names none looks as it
    ///  always did
    void load_theme();
  };

}  // namespace other

#endif  // OTHERLIB_UI_DRIVER_UI_HPP