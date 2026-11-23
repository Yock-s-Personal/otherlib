/**
 * \file ui/menu-bar/menu_item.hpp
 **/
#ifndef OTHERLIB_UI_MENU_BAR_MENU_ITEM_HPP
#define OTHERLIB_UI_MENU_BAR_MENU_ITEM_HPP

#include <string>

#include "scripting/actions/action.hpp"

namespace other {
  namespace ui {

    struct menu_item {
      std::string name;
      opt<action> action = std::nullopt;
      /// 0 unless the item was added to be taken out again by this
      natural_t id = 0;
      /// shown at the item's right edge, "Ctrl+S". only text: nothing here checks the keys
      std::string shortcut = "";
      /// false: drawn greyed, and cannot be clicked
      bool enabled = true;

      void render();
    };

  }  // namespace ui
}  // namespace other

#endif  // OTHERLIB_UI_MENU_BAR_MENU_ITEM_HPP