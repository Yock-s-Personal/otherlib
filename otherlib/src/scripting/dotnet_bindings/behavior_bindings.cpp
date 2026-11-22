/**
 * \file scripting/dotnet_bindings/behavior_bindings.cpp
 **/
#include "scripting/dotnet_bindings/behavior_bindings.hpp"

#include "core/profiler.hpp"

#include "script/behavior.hpp"
#include "script/scripting_environment.hpp"

#include "object/script_component.hpp"
#include "scene/scene.hpp"

#include "driver/driver.hpp"
#include "driver/systems/scene_system.hpp"
#include "scripting/dotnet_bindings/driver_bindings.hpp"

namespace other {
  namespace bindings {

    namespace {

      /// null when there is no scripting environment, or no script object of that id
      scripting_environment* environment_with(int64_t script_object_id) {
        if (subsystem<scripting_environment>::inert || script_object_id < 0 || static_cast<size_t>(script_object_id) >= scripting_environment::kMaxScriptObjects) {
          return nullptr;
        }
        scripting_environment* env = subsystem<scripting_environment>::get();
        return env != nullptr && env->get_object(script_object_id) != nullptr ? env : nullptr;
      }

    }  // namespace

    void native_behavior_set_runtime(const void* runtime) {
      set_behavior_runtime(static_cast<const behavior_runtime*>(runtime));
    }

    void* native_behavior_add(int64_t script_object_id, const char* type_name) {
      PROFILE_SECTION("bindings::native_behavior_add");
      scripting_environment* env = environment_with(script_object_id);
      if (env == nullptr) {
        return nullptr;
      }

      /// one of a type on an object: asking again gives the one that is there
      env->attach_dotnet_behavior(script_object_id, type_name);
      return env->get_behavior_instance(script_object_id, type_name);
    }

    nbool32 native_behavior_remove(int64_t script_object_id, const char* type_name) {
      scripting_environment* env = environment_with(script_object_id);
      if (env == nullptr || env->get_behavior_instance(script_object_id, type_name) == nullptr) {
        return false;
      }
      env->detach_dotnet_behavior(script_object_id, type_name);
      return true;
    }

    nbool32 native_behavior_set_enabled(int64_t script_object_id, const char* type_name, nbool32 enabled) {
      scripting_environment* env = environment_with(script_object_id);
      return env != nullptr && env->set_behavior_enabled(script_object_id, type_name, enabled != 0);
    }

    void attach_behavior_runtime() {
      PROFILE_SECTION("bindings::attach_behavior_runtime");
      if (subsystem<scripting_environment>::inert) {
        return;
      }
      scripting_environment* env = subsystem<scripting_environment>::get();
      if (env == nullptr) {
        return;
      }
      env->call_static_dotnet_method<void>("Other.Core.BehaviorHost", "Attach");

      /// after a rebuild of the scripts, an object a field names is one of the scene in use
      env->set_behavior_object_resolver([env](natural_t object_id) -> void* {
        driver* native_driver = detail::get_dotnet_native_driver_unchecked();
        scene* active = native_driver != nullptr ? native_driver->get_kernel().get_core_system<scene_system>().get_active_scene() : nullptr;
        if (active == nullptr) {
          return nullptr;
        }
        scene_object* named = active->find_object(object_id);
        if (named == nullptr || !active->has_component<script_component>(named)) {
          return nullptr;
        }
        return env->get_object_instance(active->get_component<script_component>(named)->script_object_id);
      });
    }

  }  // namespace bindings
}  // namespace other
