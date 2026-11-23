/**
 * \file scripting/dotnet_bindings/behavior_bindings.cpp
 **/
#include "scripting/dotnet_bindings/behavior_bindings.hpp"

#include <cstring>
#include <span>

#include "core/profiler.hpp"

#include "script/behavior.hpp"
#include "script/scripting_environment.hpp"

#include "object/script_component.hpp"
#include "scene/scene.hpp"

#include "scripting/dotnet_bindings/scene_bindings.hpp"
#include "scripting/main_thread_only.hpp"

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

      /// the script object of a scene object. -1 when there is no such object, or no environment
      int64_t script_object_of(natural_t object_id) {
        scene* active = detail::get_active_scene();
        const script_component* script = active != nullptr && active->has_object(object_id) ? active->try_get_component<script_component>(object_id) : nullptr;
        return script != nullptr && environment_with(script->script_object_id) != nullptr ? script->script_object_id : -1;
      }

      int32_t give(std::span<const uint8_t> stream, uint8_t* out_bytes, int32_t capacity) {
        const int32_t size = static_cast<int32_t>(stream.size());
        if (size > 0 && size <= capacity && out_bytes != nullptr) {
          std::memcpy(out_bytes, stream.data(), stream.size());
        }
        return size;
      }

    }  // namespace

    int32_t native_behavior_count(natural_t object_id) {
      const int64_t script_object_id = script_object_of(object_id);
      if (script_object_id < 0) {
        return 0;
      }
      return static_cast<int32_t>(subsystem<scripting_environment>::get()->get_object(script_object_id)->behavior_handles.size());
    }

    const char* native_behavior_at(natural_t object_id, int32_t index, nbool32* out_enabled, nbool32* out_built) {
      *out_enabled = false;
      *out_built = false;
      const int64_t script_object_id = script_object_of(object_id);
      if (script_object_id < 0) {
        return returned_string({});
      }
      const auto& handles = subsystem<scripting_environment>::get()->get_object(script_object_id)->behavior_handles;
      if (index < 0 || static_cast<size_t>(index) >= handles.size()) {
        return returned_string({});
      }
      *out_enabled = handles[index].enabled;
      *out_built = handles[index].instance != nullptr;
      return returned_string(handles[index].type_name);
    }

    nbool32 native_behavior_set_enabled_on(natural_t object_id, const char* type_name, nbool32 enabled) {
      const int64_t script_object_id = script_object_of(object_id);
      return script_object_id >= 0 && subsystem<scripting_environment>::get()->set_behavior_enabled(script_object_id, type_name, enabled);
    }

    int32_t native_behavior_describe(natural_t object_id, const char* type_name, uint8_t* out_bytes, int32_t capacity) {
      PROFILE_SECTION("bindings::native_behavior_describe");
      const int64_t script_object_id = script_object_of(object_id);
      if (script_object_id < 0) {
        return 0;
      }
      return give(subsystem<scripting_environment>::get()->describe_behavior_stream(script_object_id, type_name), out_bytes, capacity);
    }

    int32_t native_behavior_capture(natural_t object_id, const char* type_name, uint8_t* out_bytes, int32_t capacity) {
      PROFILE_SECTION("bindings::native_behavior_capture");
      const int64_t script_object_id = script_object_of(object_id);
      if (script_object_id < 0) {
        return 0;
      }
      return give(subsystem<scripting_environment>::get()->capture_behavior_fields(script_object_id, type_name), out_bytes, capacity);
    }

    void native_behavior_apply(natural_t object_id, const char* type_name, const uint8_t* bytes, int32_t size) {
      PROFILE_SECTION("bindings::native_behavior_apply");
      const int64_t script_object_id = script_object_of(object_id);
      if (script_object_id < 0 || bytes == nullptr || size <= 0) {
        return;
      }
      subsystem<scripting_environment>::get()->apply_behavior_fields(script_object_id, type_name, std::span<const uint8_t>{ bytes, static_cast<size_t>(size) });
    }

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
        scene* active = detail::get_active_scene();
        if (active == nullptr || !active->has_object(object_id)) {
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
