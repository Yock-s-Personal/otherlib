/**
 * \file ui/script/script_property_widget.hpp
 **/
#ifndef OTHERLIB_UI_SCRIPT_SCRIPT_PROPERTY_WIDGET_HPP
#define OTHERLIB_UI_SCRIPT_SCRIPT_PROPERTY_WIDGET_HPP

#include <string>

#include "core/defines.hpp"

#include "object/script_component.hpp"
#include "scene/scene.hpp"

#include "ui/component_widget.hpp"

namespace other {
  namespace ui {

    /// an object's behaviours, each with its switch and its saved fields, drawn from what the
    ///  script runtime says the fields are. true when a value or a switch was changed
    bool draw_script_behaviors(script_component& component, scene* in_scene);

    template <>
    struct component_widget<script_component> {
      bool operator()(const std::string_view, script_component& component, scene* in_scene, scene_object*, asset_handler* = nullptr, driver* = nullptr) {
        return draw_script_behaviors(component, in_scene);
      }
    };

  }  // namespace ui
}  // namespace other

#endif  // OTHERLIB_UI_SCRIPT_SCRIPT_PROPERTY_WIDGET_HPP
