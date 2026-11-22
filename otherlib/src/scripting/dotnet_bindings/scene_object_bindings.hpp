/**
 * \file scripting/dotnet_bindings/scene_object_bindings.hpp
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_SCENE_OBJECT_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_SCENE_OBJECT_BINDINGS_HPP

#include "core/defines.hpp"

#include "dotnet/types.hpp"

namespace other {
  namespace bindings {

    void native_get_object_id(void* object_ptr, natural_t* out_id);
    nbool32 native_validate_object_handle(natural_t object_id, uint32_t generation, uint32_t scene_id);

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_SCENE_OBJECT_BINDINGS_HPP