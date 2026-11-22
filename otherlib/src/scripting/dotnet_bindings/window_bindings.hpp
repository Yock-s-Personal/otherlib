/**
 * \file scripting/dotnet_bindings/window_bindings.hpp
 * windows declared in C#: each is a ui::script_window in a window registry, and the script
 * side keeps the object that draws it
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_WINDOW_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_WINDOW_BINDINGS_HPP

#include "core/defines.hpp"

#include "dotnet/types.hpp"

namespace other {

  class driver_ui;
  class event_system;

  namespace ui {
    class window_registry;
  }  // namespace ui

  namespace bindings {

    /// 0 when there is no registry or the name is taken
    natural_t native_ui_register_window(const char* name, const char* title, float width, float height, int32_t flags);
    void native_ui_unregister_window(natural_t id);
    /// fn is a ui::script_window::draw_fn
    void native_ui_set_window_draw_function(void* fn);

    /// any window by its name, whoever registered it. false when there is none of that name
    nbool32 native_ui_open_window(const char* name);
    nbool32 native_ui_close_window(const char* name);
    nbool32 native_ui_is_window_open(const char* name);

    /// 0 for a window that is not a script's
    natural_t native_ui_window_draw_count(const char* name);
    float native_ui_window_draw_microseconds(const char* name);

    /// an item in the main menu bar: path is the menu and any menus inside it, "File/Project".
    ///  0 when there is no menu bar to add to
    natural_t native_ui_add_menu_item(const char* path, const char* label);
    void native_ui_remove_menu_item(natural_t id);
    /// fn is a void (*)(natural_t item_id), called when an item a script added is clicked
    void native_ui_set_menu_item_click_function(void* fn);

    /// the registry script windows go in, and the event system they are made with. the
    ///  script side registers the windows it has been collecting and hands over its draw
    ///  function before this returns. menus is where the items scripts add go; without one
    ///  they add none
    void attach_script_windows(ui::window_registry* registry, event_system* events, driver_ui* menus = nullptr);
    /// the script side shuts its windows down and takes them out of the registry
    void detach_script_windows();

    /// the windows that assembly declares are made and, once a registry is attached,
    ///  registered. only called for a driver's own modules: a project's assembly declares
    ///  no editor windows yet
    void host_script_windows_of(int32_t assembly_id);

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_WINDOW_BINDINGS_HPP
