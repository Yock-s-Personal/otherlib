/**
 * \file scripting/other_abi.cpp
 **/
#include "scripting/other_abi.hpp"

#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "scene/scene.hpp"

#include "driver/driver.hpp"
#include "scripting/main_thread_only.hpp"
#include "scripting/scene_for_bindings.hpp"

namespace other {
  namespace detail {
    namespace {

      static binding_registry bindings;

      struct field_access {
        void* component = nullptr;
        const field_binding_descriptor* field = nullptr;
      };

      scene* get_active_scene() {
        /// a scene the test suite put in place of the driver's
        if (scene* bound = get_scene_for_bindings(); bound != nullptr) {
          return bound;
        }
        driver* native_driver = bindings.get_driver();
        OTHER_ASSERT(native_driver != nullptr, "Driver pointer is null in get_active_scene.");
        scene* s = native_driver->get_active_scene();
        OTHER_ASSERT(s != nullptr, "Active scene is null in get_active_scene.");
        return s;
      }

      field_access resolve_field(uint64_t obj_id, uint64_t comp_id, uint64_t field_id) {
        scene* s = get_active_scene();
        if (s == nullptr || !s->has_object(obj_id)) {
          return {};
        }

        const auto* comp_desc = bindings.find_component(comp_id);
        if (comp_desc == nullptr || comp_desc->try_get_fn == nullptr) {
          return {};
        }

        void* comp = comp_desc->try_get_fn(s, obj_id);
        if (comp == nullptr) {
          return {};
        }

        const auto* fd = comp_desc->find_field(field_id);
        if (fd == nullptr) {
          return {};
        }

        return { comp, fd };
      }

    }  // namespace
  }  // namespace detail

  void binding_registry::clear() {
    component_descriptors.clear();
    service_descriptors.clear();
    driver_ptr = nullptr;
  }

  void binding_registry::register_component(component_binding_descriptor desc) {
    const uint64_t id = desc.component_id;
    if (component_descriptors.emplace(id, std::move(desc)).second) {
      component_order.push_back(id);
    }
  }

  void binding_registry::register_service(service_binding_descriptor desc) {
    service_descriptors.emplace(desc.service_id, std::move(desc));
  }

  const component_binding_descriptor* binding_registry::find_component(uint64_t id) const {
    auto it = component_descriptors.find(id);
    return it != component_descriptors.end() ? &it->second : nullptr;
  }

  const component_binding_descriptor* binding_registry::find_component(const std::string_view name) const {
    return find_component(FNV(name));
  }

  const service_binding_descriptor* binding_registry::find_service(uint64_t id) const {
    auto it = service_descriptors.find(id);
    return it != service_descriptors.end() ? &it->second : nullptr;
  }

  namespace abi {

    void oe_init_abi(driver* drv) {
      OTHER_ASSERT(drv != nullptr, "Driver pointer is null in oe_init_abi.");
      detail::bindings.set_driver(drv);
    }

    void oe_cleanup_abi() {
      detail::bindings.clear();
    }

    binding_registry& oe_get_registry() {
      return detail::bindings;
    }

    nbool32 oe_validate_handle(uint64_t obj_id, uint32_t generation, uint32_t scene_id) {
      scene* s = detail::get_active_scene();
      if (s == nullptr || !s->has_object(obj_id)) {
        return false;
      }
      const scene_object& obj = s->get_object(obj_id);
      return obj.generation == generation;
    }

    int32_t oe_component_type_count() {
      return static_cast<int32_t>(detail::bindings.get_component_order().size());
    }

    const char* oe_component_type_at(int32_t index, uint64_t* out_id, nbool32* out_removable) {
      const std::vector<uint64_t>& order = detail::bindings.get_component_order();
      const component_binding_descriptor* desc = index >= 0 && static_cast<size_t>(index) < order.size() ? detail::bindings.find_component(order[index]) : nullptr;
      *out_id = desc != nullptr ? desc->component_id : 0;
      *out_removable = desc != nullptr && desc->removable;
      return bindings::returned_string(desc != nullptr ? std::string_view{ desc->display_name } : std::string_view{});
    }

    void oe_component_type_color(int32_t index, float* out_rgba) {
      const std::vector<uint64_t>& order = detail::bindings.get_component_order();
      const component_binding_descriptor* desc = index >= 0 && static_cast<size_t>(index) < order.size() ? detail::bindings.find_component(order[index]) : nullptr;
      const glm::vec4 color = desc != nullptr && desc->color != nullptr ? *desc->color : glm::vec4{ 0.6f, 0.6f, 0.6f, 1.f };
      out_rgba[0] = color.r;
      out_rgba[1] = color.g;
      out_rgba[2] = color.b;
      out_rgba[3] = color.a;
    }

    const char* oe_component_type_native_name(int32_t index) {
      const std::vector<uint64_t>& order = detail::bindings.get_component_order();
      const component_binding_descriptor* desc = index >= 0 && static_cast<size_t>(index) < order.size() ? detail::bindings.find_component(order[index]) : nullptr;
      return bindings::returned_string(desc != nullptr ? std::string_view{ desc->name } : std::string_view{});
    }

    nbool32 oe_has_component(uint64_t obj_id, uint64_t comp_id) {
      scene* s = detail::get_active_scene();
      if (s == nullptr || !s->has_object(obj_id)) {
        return false;
      }
      const auto* desc = detail::bindings.find_component(comp_id);
      if (desc == nullptr || desc->has_fn == nullptr) {
        return false;
      }
      return desc->has_fn(s, obj_id);
    }

    nbool32 oe_add_component(uint64_t obj_id, uint64_t comp_id) {
      PROFILE_SECTION("oe_add_component");
      scene* s = detail::get_active_scene();
      if (s == nullptr || !s->has_object(obj_id)) {
        return false;
      }
      const auto* desc = detail::bindings.find_component(comp_id);
      if (desc == nullptr || desc->add_fn == nullptr) {
        return false;
      }
      return desc->add_fn(s, obj_id) != nullptr;
    }

    nbool32 oe_remove_component(uint64_t obj_id, uint64_t comp_id) {
      PROFILE_SECTION("oe_remove_component");
      scene* s = detail::get_active_scene();
      if (s == nullptr || !s->has_object(obj_id)) {
        return false;
      }
      const auto* desc = detail::bindings.find_component(comp_id);
      if (desc == nullptr || desc->remove_fn == nullptr || !desc->removable) {
        return false;
      }
      return desc->remove_fn(s, obj_id);
    }

    nbool32 oe_get_field_bool(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, nbool32* out) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::OEBOOL || field->read_fn == nullptr) {
        return false;
      }
      /// the field is one byte; what crosses is four
      bool stored = false;
      const bool read = field->read_fn(comp, &stored);
      *out = stored;
      return read;
    }

    nbool32 oe_set_field_bool(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, nbool32 value) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::OEBOOL || field->write_fn == nullptr) {
        return false;
      }
      bool stored = value != 0;
      return field->write_fn(comp, &stored);
    }

    nbool32 oe_get_field_i32(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, int32_t* out) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::INT32 || field->read_fn == nullptr) {
        return false;
      }
      return field->read_fn(comp, out);
    }

    nbool32 oe_set_field_i32(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, int32_t value) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::INT32 || field->write_fn == nullptr) {
        return false;
      }
      return field->write_fn(comp, &value);
    }

    nbool32 oe_get_field_u32(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, uint32_t* out) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::UINT32 || field->read_fn == nullptr) {
        return false;
      }
      return field->read_fn(comp, out);
    }

    nbool32 oe_set_field_u32(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, uint32_t value) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::UINT32 || field->write_fn == nullptr) {
        return false;
      }
      return field->write_fn(comp, &value);
    }

    nbool32 oe_get_field_i64(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, int64_t* out) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::INT64 || field->read_fn == nullptr) {
        return false;
      }
      return field->read_fn(comp, out);
    }

    nbool32 oe_set_field_i64(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, int64_t value) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::INT64 || field->write_fn == nullptr) {
        return false;
      }
      return field->write_fn(comp, &value);
    }

    nbool32 oe_get_field_u64(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, uint64_t* out) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::UINT64 || field->read_fn == nullptr) {
        return false;
      }
      return field->read_fn(comp, out);
    }

    nbool32 oe_set_field_u64(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, uint64_t value) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::UINT64 || field->write_fn == nullptr) {
        return false;
      }
      return field->write_fn(comp, &value);
    }

    nbool32 oe_get_field_f32(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, float* out) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::FLOAT || field->read_fn == nullptr) {
        return false;
      }
      return field->read_fn(comp, out);
    }

    nbool32 oe_set_field_f32(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, float value) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::FLOAT || field->write_fn == nullptr) {
        return false;
      }
      return field->write_fn(comp, &value);
    }

    nbool32 oe_get_field_f64(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, double* out) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::DOUBLE || field->read_fn == nullptr) {
        return false;
      }
      return field->read_fn(comp, out);
    }

    nbool32 oe_set_field_f64(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, double value) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::DOUBLE || field->write_fn == nullptr) {
        return false;
      }
      return field->write_fn(comp, &value);
    }

    nbool32 oe_get_field_vec2(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, float* x, float* y) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::VEC2 || field->read_fn == nullptr) {
        return false;
      }
      glm::vec2 v;
      if (!field->read_fn(comp, &v)) {
        return false;
      }
      *x = v.x;
      *y = v.y;
      return true;
    }

    nbool32 oe_set_field_vec2(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, float x, float y) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::VEC2 || field->write_fn == nullptr) {
        return false;
      }
      glm::vec2 v{ x, y };
      return field->write_fn(comp, &v);
    }

    nbool32 oe_get_field_vec3(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, float* x, float* y, float* z) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::VEC3 || field->read_fn == nullptr) {
        return false;
      }
      glm::vec3 v;
      if (!field->read_fn(comp, &v)) {
        return false;
      }
      *x = v.x;
      *y = v.y;
      *z = v.z;
      return true;
    }

    nbool32 oe_set_field_vec3(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, float x, float y, float z) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::VEC3 || field->write_fn == nullptr) {
        return false;
      }
      glm::vec3 v{ x, y, z };
      return field->write_fn(comp, &v);
    }

    nbool32 oe_get_field_vec4(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, float* x, float* y, float* z, float* w) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::VEC4 || field->read_fn == nullptr) {
        return false;
      }
      glm::vec4 v;
      if (!field->read_fn(comp, &v)) {
        return false;
      }
      *x = v.x;
      *y = v.y;
      *z = v.z;
      *w = v.w;
      return true;
    }

    nbool32 oe_set_field_vec4(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, float x, float y, float z, float w) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::VEC4 || field->write_fn == nullptr) {
        return false;
      }
      glm::vec4 v{ x, y, z, w };
      return field->write_fn(comp, &v);
    }

    nbool32 oe_get_field_quat(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, float* x, float* y, float* z, float* w) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::QUATERNION || field->read_fn == nullptr) {
        return false;
      }
      glm::quat q;
      if (!field->read_fn(comp, &q)) {
        return false;
      }
      *x = q.x;
      *y = q.y;
      *z = q.z;
      *w = q.w;
      return true;
    }

    nbool32 oe_set_field_quat(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, float x, float y, float z, float w) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::QUATERNION || field->write_fn == nullptr) {
        return false;
      }
      glm::quat q{ w, x, y, z };
      return field->write_fn(comp, &q);
    }

    nbool32 oe_get_field_mat4(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, float* out_16) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::MAT4 || field->read_fn == nullptr) {
        return false;
      }
      glm::mat4 m;
      if (!field->read_fn(comp, &m)) {
        return false;
      }
      std::memcpy(out_16, &m[0][0], sizeof(float) * 16);
      return true;
    }

    nbool32 oe_set_field_mat4(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, const float* in_16) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::MAT4 || field->write_fn == nullptr) {
        return false;
      }
      glm::mat4 m;
      std::memcpy(&m[0][0], in_16, sizeof(float) * 16);
      return field->write_fn(comp, &m);
    }

    nbool32 oe_get_field_string(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, const char** out) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::STRING || field->read_fn == nullptr) {
        return false;
      }
      std::string str;
      if (!field->read_fn(comp, &str)) {
        return false;
      }
      *out = bindings::returned_string(str);
      return true;
    }

    nbool32 oe_set_field_string(uint64_t obj_id, uint64_t comp_id, uint64_t field_id, const char* value) {
      auto [comp, field] = detail::resolve_field(obj_id, comp_id, field_id);
      if (field == nullptr || field->type != value_type::STRING || field->write_fn == nullptr) {
        return false;
      }
      std::string str = static_cast<std::string>(value);
      return field->write_fn(comp, &str);
    }

  }  // namespace abi
}  // namespace other