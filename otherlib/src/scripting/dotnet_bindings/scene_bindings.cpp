/**
 * \file scripting/dotnet_bindings/scene_bindings.cpp
 **/
#include "scripting/dotnet_bindings/scene_bindings.hpp"

#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "script/scripting_environment.hpp"

#include "object/script_component.hpp"
#include "scene/scene.hpp"

#include "driver/driver.hpp"
#include "driver/systems/scene_system.hpp"
#include "scripting/dotnet_bindings/driver_bindings.hpp"
#include "scripting/main_thread_only.hpp"

namespace other {
  namespace detail {

    namespace {

      scene* scene_for_bindings = nullptr;

    }  // namespace

    void set_scene_for_bindings(scene* s) {
      scene_for_bindings = s;
    }

    scene* get_scene_for_bindings() {
      return scene_for_bindings;
    }

    scene* get_active_scene() {
      if (scene_for_bindings != nullptr) {
        return scene_for_bindings;
      }
      driver* native_driver = detail::get_dotnet_native_driver_unchecked();
      return native_driver != nullptr ? native_driver->get_kernel().get_core_system<scene_system>().get_active_scene() : nullptr;
    }

    scene* get_active_scene_checked() {
      if (scene_for_bindings != nullptr) {
        return scene_for_bindings;
      }
      OTHER_ASSERT(detail::get_dotnet_native_driver() != nullptr, "Driver pointer is null.");
      auto* env = subsystem<scripting_environment>::get();
      OTHER_ASSERT(env != nullptr, "Scripting environment is not initialized.");
      auto* active_scene = detail::get_dotnet_native_driver()->get_kernel().get_core_system<scene_system>().get_active_scene();
      OTHER_ASSERT(active_scene != nullptr, "Active scene is null.");
      return active_scene;
    }

  }  // namespace detail
  namespace bindings {

    natural_t native_scene_create_object(const char* name, float x, float y, float z) {
      PROFILE_SECTION("native_scene_create_object");
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        CORE_LOG_ERROR("No active scene to create object in.");
        return 0;
      }
      std::string name_str = name;
      scene_object& obj = active_scene->create_object(name_str, glm::vec3(x, y, z));
      return obj.id;
    }

    void native_scene_destroy_object(natural_t id) {
      PROFILE_SECTION("native_scene_destroy_object");
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return;
      }
      active_scene->destroy_object(id);
    }

    nbool32 native_scene_has_object(natural_t id) {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return false;
      }
      return active_scene->has_object(id);
    }

    const char* native_scene_get_object_name(natural_t id) {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return returned_string("");
      }
      const scene_object& obj = active_scene->get_object(id);
      return returned_string(obj.name);
    }

    void native_scene_set_object_name(natural_t id, const char* name) {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return;
      }
      active_scene->rename_object(id, (std::string)name);
    }

    void native_scene_get_object_ids(natural_t* out_ids, int32_t* out_count, int32_t max_count) {
      PROFILE_SECTION("native_scene_get_object_ids");
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        *out_count = 0;
        return;
      }
      auto ids = active_scene->get_all_object_ids();
      int32_t count = std::min((int32_t)ids.size(), max_count);
      for (int32_t i = 0; i < count; ++i) {
        out_ids[i] = ids[i];
      }
      *out_count = count;
    }

    natural_t native_scene_get_object_count() {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return 0;
      }
      return active_scene->get_object_count();
    }

    natural_t native_scene_find_object_by_name(const char* name) {
      PROFILE_SECTION("native_scene_find_object_by_name");
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return 0;
      }
      std::string name_str = name;
      scene_object* obj = active_scene->find_object(name_str);
      if (obj == nullptr) {
        return 0;
      }
      return obj->id;
    }

    void* native_scene_object_instance(natural_t id) {
      scene* active_scene = detail::get_active_scene();
      if (active_scene == nullptr || subsystem<scripting_environment>::inert || !active_scene->has_object(id)) {
        return nullptr;
      }
      scene_object* named = active_scene->find_object(id);
      if (named == nullptr || named == &active_scene->root_object() || !active_scene->has_component<script_component>(named)) {
        return nullptr;
      }
      return subsystem<scripting_environment>::get()->get_object_instance(active_scene->get_component<script_component>(named)->script_object_id);
    }

    nbool32 native_scene_set_parent(natural_t id, natural_t parent_id, nbool32 keep_world) {
      PROFILE_SECTION("native_scene_set_parent");
      scene* active_scene = detail::get_active_scene();
      if (active_scene == nullptr || !active_scene->has_object(id)) {
        return false;
      }
      const natural_t new_parent = parent_id != 0 ? parent_id : active_scene->root_object().id;
      if (!active_scene->has_object(new_parent)) {
        return false;
      }

      /// reparent keeps the object where it is in the world by working out a new local
      ///  transform; without keep_world the one it had is put back
      const transform local = active_scene->get_transform(id);
      if (!active_scene->reparent_object(id, new_parent)) {
        return false;
      }
      if (!keep_world) {
        active_scene->get_transform(id) = local;
      }
      return true;
    }

    natural_t native_scene_find_object_with_tag(const char* tag) {
      PROFILE_SECTION("native_scene_find_object_with_tag");
      scene* active_scene = detail::get_active_scene();
      if (active_scene == nullptr || tag == nullptr) {
        return 0;
      }
      const scene_object* found = active_scene->find_object_with_tag(tag);
      return found != nullptr ? found->id : 0;
    }

    void native_scene_find_objects_with_tag(const char* tag, natural_t* out_ids, int32_t* out_count, int32_t max_count) {
      PROFILE_SECTION("native_scene_find_objects_with_tag");
      *out_count = 0;
      scene* active_scene = detail::get_active_scene();
      if (active_scene == nullptr || tag == nullptr) {
        return;
      }
      for (const natural_t id : active_scene->get_all_object_ids()) {
        if (*out_count >= max_count) {
          break;
        }
        if (active_scene->object_has_tag(id, tag)) {
          out_ids[(*out_count)++] = id;
        }
      }
    }

    natural_t native_scene_get_parent_id(natural_t id) {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return 0;
      }
      const scene_object* parent = active_scene->get_parent(id);
      return parent != nullptr ? parent->id : 0;
    }

    void native_scene_get_children_ids(natural_t id, natural_t* out_ids, int32_t* out_count, int32_t max_count) {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        *out_count = 0;
        return;
      }
      auto ids = active_scene->get_children_ids(id);
      int32_t count = std::min((int32_t)ids.size(), max_count);
      for (int32_t i = 0; i < count; ++i) {
        out_ids[i] = ids[i];
      }
      *out_count = count;
    }

    nbool32 native_scene_object_has_tag(natural_t id, const char* tag) {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return false;
      }
      return active_scene->object_has_tag(id, (std::string)tag);
    }

    void native_scene_add_object_tag(natural_t id, const char* tag) {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return;
      }
      active_scene->add_object_tag(id, (std::string)tag);
    }

    void native_scene_remove_object_tag(natural_t id, const char* tag) {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return;
      }
      active_scene->remove_object_tag(id, (std::string)tag);
    }

    nbool32 native_scene_get_object_visible(natural_t id) {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return false;
      }
      return active_scene->get_object(id).visible;
    }

    void native_scene_set_object_visible(natural_t id, nbool32 visible) {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr) {
        return;
      }
      active_scene->set_object_visible(id, visible != 0);
    }

    uint64_t native_scene_revision() {
      const scene* active_scene = detail::get_active_scene();
      return active_scene != nullptr ? active_scene->revision() : 0;
    }

    const char* native_scene_get_name() {
      const scene* active_scene = detail::get_active_scene();
      return returned_string(active_scene != nullptr ? std::string_view{ active_scene->name } : std::string_view{});
    }

    void native_scene_get_top_ids(natural_t* out_ids, int32_t* out_count, int32_t max_count) {
      *out_count = 0;
      scene* active_scene = detail::get_active_scene();
      if (active_scene == nullptr) {
        return;
      }
      for (const natural_t id : active_scene->get_children_ids(active_scene->root_object().id)) {
        if (*out_count >= max_count) {
          break;
        }
        out_ids[(*out_count)++] = id;
      }
    }

  }  // namespace bindings
}  // namespace other