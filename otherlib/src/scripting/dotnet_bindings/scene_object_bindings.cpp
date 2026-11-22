/**
 * \file scripting/dotnet_bindings/scene_object_bindings.cpp
 **/
#include "scripting/dotnet_bindings/scene_object_bindings.hpp"

#include "script/scripting_environment.hpp"

#include "object/scene_object.hpp"
#include "scene/scene.hpp"

#include "scripting/dotnet_bindings/scene_bindings.hpp"

namespace other {
  namespace bindings {

    void native_get_object_id(void* object_ptr, natural_t* out_id) {
      OTHER_ASSERT(object_ptr != nullptr, "Native object pointer is null.");
      OTHER_ASSERT(out_id != nullptr, "Output ID pointer is null.");

      scripting_environment* env = subsystem<scripting_environment>::get();
      OTHER_ASSERT(env != nullptr, "Scripting environment is not initialized.");

      scene_object* obj = (scene_object*)object_ptr;
      *out_id = obj->id;
    }

    nbool32 native_validate_object_handle(natural_t object_id, uint32_t generation, uint32_t scene_id) {
      scene* active_scene = detail::get_active_scene_checked();
      if (active_scene == nullptr || !active_scene->has_object(object_id)) {
        return false;
      }

      /// a script's handle does not carry its scene yet: 0 stands for the active one
      scene_object_handle handle{ scene_id != 0 ? static_cast<natural_t>(scene_id) : active_scene->id, object_id, generation };
      return validate_handle(active_scene, handle);
    }

  }  // namespace bindings
}  // namespace other