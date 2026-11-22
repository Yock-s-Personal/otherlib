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

  }  // namespace bindings
}  // namespace other