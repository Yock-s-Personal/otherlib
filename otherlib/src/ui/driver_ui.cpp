/**
 * \file ui/driver_ui.cpp
 **/
#include "ui/driver_ui.hpp"

#include <algorithm>

#include "core/defines.hpp"
#include "core/profiler.hpp"
#include "file/path_helpers.hpp"

#include "driver/driver.hpp"
#include "scripting/dotnet_bindings/window_bindings.hpp"
#include "theme/font.hpp"
#include "theme/theme.hpp"
#include "ui/console/console.hpp"
#include "ui/type_database.hpp"

namespace other {
  namespace {

    bool erase_menu_item(ui::menu& menu, natural_t id) {
      const auto found = std::ranges::find(menu.items, id, &ui::menu_item::id);
      if (found != menu.items.end()) {
        menu.items.erase(found);
        return true;
      }
      return std::ranges::any_of(menu.sub_menus, [id](ui::menu& sub_menu) { return erase_menu_item(sub_menu, id); });
    }

  }  // namespace

  void driver_ui::initialize() {
    PROFILE_SECTION("driver_ui::initialize");
    component_reg = make_scope<ui::component_widget_registry>();
    OTHER_ASSERT(component_reg != nullptr, "Failed to create component widget registry in driver UI initialization.");

    load_theme();

    register_window("type-database", make_scope<ui::type_database>(*driver_ptr->get_event_system()), ui::window_source::BUILTIN);
    register_window("console", make_scope<ui::console_window>(*driver_ptr->get_event_system(), driver_ptr), ui::window_source::BUILTIN);

    main_menu_bar.main_menu_bar = true;
    main_menu_bar_open = driver_ptr->configuration().get_value<bool>("ui.enable-environment-menu-bar", false);

    /// scripts were loaded before there was anywhere to put a window: theirs arrive now
    bindings::attach_script_windows(&windows, driver_ptr->get_event_system().get(), this);
  }

  void driver_ui::render() {
    PROFILE_SECTION("driver_ui::render");
    if (main_menu_bar_open) {
      PROFILE_SECTION("driver_ui::render--main_menu_bar");
      auto menus =
        main_menu_bar_menus |
        std::views::transform(&main_menu_bar_menu::menu) |
        std::ranges::to<ostd::vector<ui::menu>>();
      main_menu_bar.render(menus);
    }

    windows.render();
  }

  void driver_ui::shutdown() {
    PROFILE_SECTION("driver_ui::shutdown");
    bindings::detach_script_windows();
    windows.clear();
    ui_fonts().clear();
  }

  void driver_ui::load_theme() {
    PROFILE_SECTION("driver_ui::load_theme");
    const std::string setting = driver_ptr->configuration().get_value<std::string>("ui.theme", "");
    if (setting.empty() || ImGui::GetCurrentContext() == nullptr) {
      return;
    }

    /// said by the resolver when the file is not there
    const asset_path file = resolve_asset_path(setting);
    if (!file.found()) {
      return;
    }

    ui_theme theme;
    const theme_load_result loaded = load_theme_file(file.resolved, theme);
    for (const std::string& problem : loaded.problems) {
      CORE_LOG_WARN("Theme '{}': {}", setting, problem);
    }
    if (!loaded.loaded) {
      CORE_LOG_ERROR("The theme '{}' was not loaded: the UI looks as it does with none.", setting);
      return;
    }
    apply_theme(theme, ImGui::GetStyle());

    /// a font added after ImGui has built its fonts would never be drawn
    ostd::vector<std::string> font_problems;
    if (ImGui::GetIO().Fonts->IsBuilt()) {
      font_problems.push_back("[typography] was not read: ImGui's fonts were built before the theme was loaded");
    } else {
      ui_fonts().load(theme.fonts, file.resolved, font_problems);
    }
    for (const std::string& problem : font_problems) {
      CORE_LOG_WARN("Theme '{}': {}", setting, problem);
    }
    CORE_LOG_INFO("Theme '{}' loaded: {} colours set, {} font roles", setting, loaded.colors_set, ui_fonts().loaded_count());
  }

  void driver_ui::available_ui_window_menu() {
    PROFILE_SECTION("driver_ui::available_ui_window_menu");
    bool any_listed = false;
    for (uint8_t source = 0; source < static_cast<uint8_t>(ui::window_source::NUM_SOURCES); ++source) {
      bool first_of_source = true;
      for (const auto& window : windows.entries()) {
        if (static_cast<uint8_t>(window.source) != source) {
          continue;
        }

        if (first_of_source && any_listed) {
          ImGui::Separator();
        }
        first_of_source = false;
        any_listed = true;

        if (!ImGui::MenuItem(window.name.c_str(), nullptr, window.open)) {
          continue;
        }
        if (window.open) {
          windows.close(window.name);
        } else {
          windows.open(window.name);
        }
      }
    }
  }

  ostd::vector<std::string> driver_ui::get_open_window_names() const {
    return windows.open_names();
  }

  ostd::vector<std::string> driver_ui::get_available_window_names() const {
    return windows.names();
  }

  void driver_ui::open_window(const std::string_view window_name) {
    PROFILE_SECTION("driver_ui::open_window");
    CORE_LOG_DEBUG("Request to open UI window: {}", window_name);
    if (!windows.open(window_name)) {
      CORE_LOG_ERROR("There is no UI window named '{}' to open.", window_name);
    }
  }

  void driver_ui::close_window(const std::string_view window_name) {
    PROFILE_SECTION("driver_ui::close_window");
    CORE_LOG_DEBUG("Request to close UI window: {}", window_name);
    if (!windows.close(window_name)) {
      CORE_LOG_ERROR("There is no UI window named '{}' to close.", window_name);
    }
  }

  void driver_ui::close_all_windows() {
    PROFILE_SECTION("driver_ui::close_all_windows");
    CORE_LOG_DEBUG("Request to close all UI windows");
    windows.close_all();
  }

  bool driver_ui::any_window_open() const {
    return windows.any_open();
  }

  bool driver_ui::is_window_open(const std::string_view window_name) const {
    return windows.is_open(window_name);
  }

  void driver_ui::register_main_menu_bar_sub_menu(const std::string_view menu_name, const ui::menu& sub_menu) {
    PROFILE_SECTION("driver_ui::register_main_menu_bar_sub_menu");
    natural_t hash = FNV(menu_name);
    auto it = std::ranges::find_if(main_menu_bar_menus, [hash](const auto& menu) { return menu.hash == hash; });
    if (it == main_menu_bar_menus.end()) {
      register_main_menu_bar_menu(menu_name);
      it = std::ranges::find_if(main_menu_bar_menus, [hash](const auto& menu) { return menu.hash == hash; });
      OTHER_ASSERT(it != main_menu_bar_menus.end(), "Failed to find or register main menu bar menu with name '{}'", menu_name);
    }

    it->menu.sub_menus.push_back(sub_menu);
    CORE_LOG_DEBUG("Registered main menu bar sub-menu: {}:{} [{}]", menu_name, sub_menu.name, hash);
  }

  void driver_ui::register_main_menu_bar_menu(const std::string_view menu_name) {
    PROFILE_SECTION("driver_ui::register_main_menu_bar_menu");
    natural_t hash = FNV(menu_name);
    if (std::ranges::find_if(main_menu_bar_menus, [hash](const auto& item) { return item.hash == hash; }) != main_menu_bar_menus.end()) {
      CORE_LOG_ERROR("Main menu bar menu with name '{}' is already registered.", menu_name);
      return;
    }

    main_menu_bar_menus.push_back({
      .hash = hash,
      .menu = {
        .name = std::string(menu_name),
        .sub_menus = {},
        .items = {},
      },
    });
    CORE_LOG_DEBUG("Registered main menu bar menu: {} [{}]", menu_name, hash);
  }

  void driver_ui::register_main_menu_bar_menu(const ui::menu& menu) {
    PROFILE_SECTION("driver_ui::register_main_menu_bar_menu");
    natural_t hash = FNV(menu.name);
    if (std::ranges::find_if(main_menu_bar_menus, [hash](const auto& item) { return item.hash == hash; }) != main_menu_bar_menus.end()) {
      CORE_LOG_ERROR("Main menu bar menu with name '{}' is already registered.", menu.name);
      return;
    }

    main_menu_bar_menus.push_back({
      .hash = hash,
      .menu = menu,
    });
    CORE_LOG_DEBUG("Registered main menu bar menu: {} [{}]", menu.name, hash);
  }

  void driver_ui::register_main_menu_bar_menu_item(const std::string_view menu_name, const ui::menu_item& item) {
    PROFILE_SECTION("driver_ui::register_main_menu_bar_menu_item");
    natural_t hash = FNV(menu_name);
    auto it = std::ranges::find_if(main_menu_bar_menus, [hash](const auto& menu) { return menu.hash == hash; });
    if (it == main_menu_bar_menus.end()) {
      register_main_menu_bar_menu(menu_name);
      it = std::ranges::find_if(main_menu_bar_menus, [hash](const auto& menu) { return menu.hash == hash; });
      OTHER_ASSERT(it != main_menu_bar_menus.end(), "Failed to find or register main menu bar menu with name '{}'", menu_name);
    }

    it->menu.items.push_back(item);
    CORE_LOG_DEBUG("Registered main menu bar menu item: {}:{} [{}]", menu_name, item.name, hash);
  }

  natural_t driver_ui::add_menu_item(const std::string_view path, const std::string_view label, std::function<void(natural_t)> on_click) {
    PROFILE_SECTION("driver_ui::add_menu_item");
    const size_t first = path.find('/');
    const std::string_view top = path.substr(0, first);
    if (top.empty()) {
      CORE_LOG_ERROR("The menu item '{}' was not added: '{}' names no menu.", label, path);
      return 0;
    }

    const natural_t hash = FNV(top);
    auto it = std::ranges::find_if(main_menu_bar_menus, [hash](const auto& menu) { return menu.hash == hash; });
    if (it == main_menu_bar_menus.end()) {
      register_main_menu_bar_menu(top);
      it = std::ranges::find_if(main_menu_bar_menus, [hash](const auto& menu) { return menu.hash == hash; });
      OTHER_ASSERT(it != main_menu_bar_menus.end(), "Failed to find or register main menu bar menu with name '{}'", top);
    }

    /// the rest of the path is menus inside that one, made as they are first named
    ui::menu* target = &it->menu;
    std::string_view rest = first == std::string_view::npos ? std::string_view{} : path.substr(first + 1);
    while (!rest.empty()) {
      const size_t next = rest.find('/');
      const std::string_view part = rest.substr(0, next);
      rest = next == std::string_view::npos ? std::string_view{} : rest.substr(next + 1);
      if (part.empty()) {
        continue;
      }

      auto sub_menu = std::ranges::find(target->sub_menus, part, &ui::menu::name);
      if (sub_menu == target->sub_menus.end()) {
        target->sub_menus.push_back(ui::menu{ .name = std::string{ part } });
        sub_menu = std::prev(target->sub_menus.end());
      }
      target = &*sub_menu;
    }

    const natural_t id = ++last_menu_item_id;
    target->items.push_back(ui::menu_item{
      .name = std::string{ label },
      .action = action{ std::function<void()>([id, on_click = std::move(on_click)]() { on_click(id); }) },
      .id = id,
    });
    CORE_LOG_DEBUG("Added menu item: {} [{}]", path, id);
    return id;
  }

  void driver_ui::remove_menu_item(natural_t id) {
    PROFILE_SECTION("driver_ui::remove_menu_item");
    if (id == 0) {
      return;
    }
    for (main_menu_bar_menu& menu : main_menu_bar_menus) {
      if (erase_menu_item(menu.menu, id)) {
        return;
      }
    }
  }

  natural_t driver_ui::register_window(const std::string_view name, scope<ui_window> window, ui::window_source source) {
    PROFILE_SECTION("driver_ui::register_window");
    OTHER_ASSERT(window != nullptr, "Window '{}' registered with a null window pointer.", name);
    OTHER_ASSERT(driver_ptr != nullptr, "Driver pointer is null in driver UI register_window.");
    window->set_driver_ptr(driver_ptr);
    return windows.add(name, std::move(window), source);
  }

  void driver_ui::unregister_window(natural_t id) {
    PROFILE_SECTION("driver_ui::unregister_window");
    if (!windows.remove(id)) {
      CORE_LOG_ERROR("Failed to unregister window with ID {}: no such window registered.", id);
    }
  }

  void driver_ui::unregister_window(const std::string_view name) {
    unregister_window(FNV(name));
  }

  event_system& driver_ui::events() {
    OTHER_ASSERT(driver_ptr != nullptr, "Driver pointer is null in driver UI.");
    return *driver_ptr->get_event_system();
  }

}  // namespace other
