/**
 * \file scripting/dotnet_bindings/inspector_bindings.cpp
 **/
#include "scripting/dotnet_bindings/inspector_bindings.hpp"

#include <cstring>
#include <string>

#include <glm/glm.hpp>
#include <imgui/imgui.h>

#include "core/profiler.hpp"

#include "object/scene_object.hpp"
#include "scene/scene.hpp"

#include "driver/driver.hpp"
#include "driver/systems/asset_system.hpp"
#include "scripting/dotnet_bindings/driver_bindings.hpp"
#include "scripting/dotnet_bindings/scene_bindings.hpp"
#include "scripting/other_abi.hpp"
#include "theme/colors.hpp"
#include "ui/asset_picker.hpp"
#include "ui/inspector_widgets.hpp"

namespace other {
  namespace bindings {

    nbool32 native_ui_object_header(char* name_buffer, int32_t buffer_size, natural_t object_id) {
      if (name_buffer == nullptr || buffer_size <= 0) {
        return false;
      }
      return ui::inspector::draw_object_header_editable(name_buffer, static_cast<uint32_t>(buffer_size), object_id, ui::colors::scene_object::kSignature);
    }

    nbool32 native_ui_begin_component_section(const char* name, float r, float g, float b, float a, nbool32 removable, nbool32* out_remove) {
      bool remove = false;
      const bool open = ui::inspector::begin_component_section(name, glm::vec4{ r, g, b, a }, { .removable = removable != 0 }, &remove);
      *out_remove = remove;
      return open;
    }

    void native_ui_end_component_section() {
      ui::inspector::end_component_section();
    }

    nbool32 native_ui_component_fields(natural_t object_id, natural_t component_id) {
      PROFILE_SECTION("bindings::native_ui_component_fields");
      /// the rows read the driver: its assets, and the editor each kind of field has
      driver* drvr = detail::get_dotnet_native_driver_unchecked();
      scene* active = detail::get_active_scene();
      const component_binding_descriptor* described = abi::oe_get_registry().find_component(component_id);
      scene_object* object = active != nullptr ? active->find_object(object_id) : nullptr;
      if (drvr == nullptr || object == nullptr || described == nullptr || described->draw_fn == nullptr) {
        return false;
      }
      return described->draw_fn(active, object, drvr);
    }

    nbool32 native_ui_add_component_button(const char* label) {
      return ui::inspector::draw_add_component_button(label);
    }

    nbool32 native_ui_property_bool(const char* label, nbool32* value) {
      bool on = *value != 0;
      const bool changed = ui::inspector::property_bool(label, on);
      *value = on;
      return changed;
    }

    nbool32 native_ui_property_float(const char* label, float* value, float speed) {
      return ui::inspector::property_float(label, *value, speed);
    }

    nbool32 native_ui_property_int32(const char* label, int32_t* value) {
      return ui::inspector::property_int32(label, *value);
    }

    nbool32 native_ui_property_uint32(const char* label, uint32_t* value) {
      return ui::inspector::property_uint32(label, *value);
    }

    nbool32 native_ui_property_int64(const char* label, int64_t* value) {
      return ui::inspector::property_int64(label, *value);
    }

    nbool32 native_ui_property_uint64(const char* label, uint64_t* value) {
      return ui::inspector::property_uint64(label, *value);
    }

    nbool32 native_ui_property_vec2(const char* label, float* xy, float speed) {
      glm::vec2 value{ xy[0], xy[1] };
      const bool changed = ui::inspector::property_vec2(label, value, speed);
      std::memcpy(xy, &value, sizeof(value));
      return changed;
    }

    nbool32 native_ui_property_vec3(const char* label, float* xyz, float speed) {
      glm::vec3 value{ xyz[0], xyz[1], xyz[2] };
      const bool changed = ui::inspector::property_vec3(label, value, speed);
      std::memcpy(xyz, &value, sizeof(value));
      return changed;
    }

    nbool32 native_ui_property_vec4(const char* label, float* xyzw, float speed) {
      glm::vec4 value{ xyzw[0], xyzw[1], xyzw[2], xyzw[3] };
      const bool changed = ui::inspector::property_vec4(label, value, speed);
      std::memcpy(xyzw, &value, sizeof(value));
      return changed;
    }

    nbool32 native_ui_property_text(const char* label, char* buffer, int32_t buffer_size) {
      if (buffer == nullptr || buffer_size <= 0) {
        return false;
      }
      return ui::inspector::property_text(label, buffer, static_cast<uint32_t>(buffer_size));
    }

    nbool32 native_ui_asset_drop_target(char* out_path, int32_t capacity) {
      if (out_path == nullptr || capacity <= 0 || !ImGui::BeginDragDropTarget()) {
        return false;
      }

      std::string path = "";
      if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(ui::inspector::kAssetDragDropPayloadType); dropped != nullptr && dropped->Data != nullptr) {
        const auto* data = static_cast<const ui::inspector::asset_drag_drop_payload*>(dropped->Data);
        path = data->path;
        /// an asset the engine already holds is dragged by its id: its path is asked for
        if (driver* drvr = detail::get_dotnet_native_driver_unchecked(); path.empty() && data->handler_asset_id != 0 && drvr != nullptr) {
          const auto& assets = drvr->get_kernel().get_core_system<asset_system>().get_asset_manager();
          if (const asset* held = assets->get_loaded_asset(data->handler_asset_id); held != nullptr) {
            path = held->load_path.generic_string();
          }
        }
      }
      ImGui::EndDragDropTarget();

      if (path.empty() || path.size() >= static_cast<size_t>(capacity)) {
        return false;
      }
      std::memcpy(out_path, path.c_str(), path.size() + 1);
      return true;
    }

  }  // namespace bindings
}  // namespace other
