/**
 * \file ui/component_widget.hpp
 **/
#ifndef OTHERLIB_UI_COMPONENT_WIDGET_HPP
#define OTHERLIB_UI_COMPONENT_WIDGET_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include "core/profiler.hpp"
#include "math/orthonormal_basis.hpp"
#include "serialization/reflection.hpp"

#include "script/script_object.hpp"

#include "object/scene_object.hpp"
#include "scene/scene.hpp"

#include "driver/driver.hpp"
#include "driver/systems/asset_system.hpp"
#include "theme/colors.hpp"
#include "ui/asset_picker.hpp"
#include "ui/inspector_widgets.hpp"

#include "asset/asset.hpp"
#include "asset/asset_handler.hpp"

namespace other {
  namespace ui {

    namespace detail {

      template <typename T>
      field_flags read_field_flags(T field) {
        field_flags f;
        if constexpr (refl::descriptor::has_attribute<attr::serializable>(field)) {
          constexpr auto& s = refl::descriptor::get_attribute<attr::serializable>(field);
          f.display_name = s.display_name;  // std::string_view; empty ⇒ use field name
          f.read_only = !s.editable;        // serializable(false) ⇒ read-only (transform.local_basis)
        }
        if constexpr (refl::descriptor::has_attribute<attr::asset_identifier_field>(field)) {
          f.asset_type = refl::descriptor::get_attribute<attr::asset_identifier_field>(field).asset_type;
        }
        if constexpr (refl::descriptor::has_attribute<attr::clamp>(field)) {
          const auto& c = refl::descriptor::get_attribute<attr::clamp>(field);
          f.has_range = true;
          f.range = glm::vec2{
            static_cast<float>(c.min),
            static_cast<float>(c.max)
          };
        }
        return f;
      }

      template <typename FT>
      bool draw_field(std::string_view label, FT& value, const field_context& ctx) {
        const type_key key = type_key_of<FT>();

        /// asset-id members must not fall through to the raw uint64 editor - they get
        ///  the slot + picker (attr::asset_identifier_field carries the type)
        if constexpr (std::same_as<FT, natural_t>) {
          if (ctx.flags.asset_type != asset::EMPTY) {
            return inspector::property_asset_field(label, value, ctx.flags.asset_type, ctx.asset_handler_ptr, ctx.driver_ptr);
          }
        }

        auto& field_editors = ctx.driver_ptr->get_field_editors();
        if (field_editors.has(key)) {
          return field_editors.edit(key, label, &value, ctx);
        }

        if constexpr (reflected_type<FT>) {
          bool changed = false;
          for_each(refl::reflect(value).members, [&](auto sub) {
            field_context child = ctx;
            child.flags = read_field_flags(sub);
            changed |= draw_field(child.flags.display_name.empty() ? std::string{ sub.name } : child.flags.display_name,
                                  sub(value), child);
          });
          return changed;
        }

        if constexpr (is_container<FT>) {
          bool changed = false;
          for (auto& item : value) {
            field_context child = ctx;
            changed |= draw_field("", item, child);
          }
          return changed;
        }

        bool modified = field_editors.edit(key, label, &value, ctx);  // draws "<unsupported type>"
        // clamp value if necessary

        if (ctx.flags.has_range) {
          if constexpr (is_linear_algebra_type<FT> &&
                        !(std::is_same_v<FT, glm::mat4> || std::is_same_v<FT, glm::mat3> || std::is_same_v<FT, glm::mat2>)) {
            for (int i = 0; i < value.length(); ++i) {
              if (value[i] < (float)ctx.flags.range.x) {
                value[i] = (float)ctx.flags.range.x;
                modified = true;
              }
              if (value[i] > (float)ctx.flags.range.y) {
                value[i] = (float)ctx.flags.range.y;
                modified = true;
              }
            }
          } else if constexpr (std::is_arithmetic_v<FT>) {
            if (value < static_cast<FT>(ctx.flags.range.x)) {
              value = static_cast<FT>(ctx.flags.range.x);
              modified = true;
            }
            if (value > static_cast<FT>(ctx.flags.range.y)) {
              value = static_cast<FT>(ctx.flags.range.y);
              modified = true;
            }
          } else if constexpr (std::is_floating_point_v<FT>) {
            if (value < static_cast<FT>(ctx.flags.range.x)) {
              value = static_cast<FT>(ctx.flags.range.x);
              modified = true;
            }
            if (value > static_cast<FT>(ctx.flags.range.y)) {
              value = static_cast<FT>(ctx.flags.range.y);
              modified = true;
            }
          }
        }

        return modified;
      }

    }  // namespace detail

    template <typename T>
    struct component_widget {
      bool operator()(const std::string_view name, T& component, scene* active_scene, scene_object* object, asset_handler* handler = nullptr, driver* drvr = nullptr) {
        OTHER_ASSERT(drvr != nullptr, "component_widget needs a driver for '{}'", name);
        PROFILE_SECTION("component_widget::operator()");
        bool changed = false;
        for_each(refl::reflect(component).members, [&](auto field) {
          field_context ctx{ active_scene, object, handler, drvr, detail::read_field_flags(field) };
          std::string label = ctx.flags.display_name.empty() ? std::string{ field.name } : ctx.flags.display_name;
          changed |= detail::draw_field(label, field(component), ctx);
        });
        return changed;
      }
    };

  }  // namespace ui
}  // namespace other

#endif  // OTHERLIB_UI_COMPONENT_WIDGET_HPP