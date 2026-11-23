/**
 * \file ui/component_drawer.hpp
 * a component's fields as the inspector's rows, for whoever draws an inspector: the general
 * drawer walks the component's reflected members, and the two components below have rows
 * written for them by hand
 **/
#ifndef OTHERLIB_UI_COMPONENT_DRAWER_HPP
#define OTHERLIB_UI_COMPONENT_DRAWER_HPP

#include <algorithm>
#include <array>

#include <imgui/imgui.h>

#include "object/audio_source_component.hpp"
#include "object/render_component.hpp"
#include "object/scene_object.hpp"
#include "scene/scene.hpp"

#include "driver/driver.hpp"
#include "driver/systems/asset_system.hpp"
#include "ui/asset_picker.hpp"
#include "ui/component_widget.hpp"
#include "ui/inspector_widgets.hpp"

namespace other {
  namespace ui {

    /// render components draw by hand: model/material go through the asset slot + picker,
    ///  and the reflected walk would also surface obj_model/last_* internals
    template <>
    struct component_widget<render_component> {
      bool operator()(const std::string_view, render_component& comp, scene*, scene_object*, asset_handler* handler, driver* drvr) {
        OTHER_ASSERT(drvr != nullptr, "component_widget<render_component> needs a driver");
        bool changed = inspector::property_bool("Visible", comp.visible);
        changed |= inspector::property_vec4("Tint", comp.tint, 0.01f);
        /// prepare_render_data validates the new ids and rebuilds obj_model once loaded
        changed |= inspector::property_asset_field("Model", comp.model_asset_id, asset::MODEL_SOURCE, handler, drvr);
        changed |= inspector::property_asset_field("Material", comp.material_asset_id, asset::MATERIAL, handler, drvr);
        return changed;
      }
    };

    /// audio sources draw by hand: the clip goes through the asset slot and the bus wants a combo
    template <>
    struct component_widget<audio_source_component> {
      bool operator()(const std::string_view, audio_source_component& comp, scene*, scene_object*, asset_handler* handler, driver* drvr) {
        OTHER_ASSERT(drvr != nullptr, "component_widget<audio_source_component> needs a driver");
        bool changed = inspector::property_asset_field("Clip", comp.clip_asset_id, asset::AUDIO, handler, drvr);

        changed |= inspector::property_bool("Playing", comp.playing);
        changed |= inspector::property_bool("Looping", comp.looping);
        changed |= inspector::property_float("Volume", comp.volume, 0.01f);
        changed |= inspector::property_float("Pitch", comp.pitch, 0.01f);

        constexpr std::array<const char*, 4> kBusNames = { "Master", "Music", "SFX", "UI" };
        int bus_index = static_cast<int>(std::min<uint32_t>(comp.bus, static_cast<uint32_t>(kBusNames.size()) - 1));
        if (ImGui::Combo("Bus", &bus_index, kBusNames.data(), static_cast<int>(kBusNames.size()))) {
          comp.bus = static_cast<uint32_t>(bus_index);
          changed = true;
        }

        changed |= inspector::property_bool("Spatial", comp.spatial);
        if (comp.spatial) {
          changed |= inspector::property_float("Min Distance", comp.min_distance, 0.1f);
          changed |= inspector::property_float("Max Distance", comp.max_distance, 1.f);
          changed |= inspector::property_float("Doppler", comp.doppler_factor, 0.01f);
        }
        return changed;
      }
    };

    /// the rows of the object's component of that type. true when one of them was changed.
    ///  nothing is drawn, and false, when the object has none
    template <typename T>
    bool draw_component_fields(scene* in_scene, scene_object* object, driver* drvr) {
      OTHER_ASSERT(in_scene != nullptr && object != nullptr && drvr != nullptr, "A component's fields are drawn for an object of a scene, by a driver.");
      PROFILE_SECTION("ui::draw_component_fields");
      T* comp = in_scene->try_get_component<T>(object->id);
      if (comp == nullptr) {
        return false;
      }
      asset_handler* handler = drvr->get_kernel().get_core_system<asset_system>().get_asset_manager().get();
      OTHER_ASSERT(handler != nullptr, "Asset handler is null");
      return component_widget<T>{}("", *comp, in_scene, object, handler, drvr);
    }

  }  // namespace ui
}  // namespace other

#endif  // OTHERLIB_UI_COMPONENT_DRAWER_HPP
