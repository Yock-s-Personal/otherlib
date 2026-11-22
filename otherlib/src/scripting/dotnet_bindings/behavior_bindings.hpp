/**
 * \file scripting/dotnet_bindings/behavior_bindings.hpp
 * what C# asks the engine about behaviours: it hands over the functions the engine runs
 * them through, and adds, removes and switches them by asking, so the engine's record of an
 * object's behaviours is the only one
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_BEHAVIOR_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_BEHAVIOR_BINDINGS_HPP

#include "core/defines.hpp"

#include "dotnet/types.hpp"

namespace other {
  namespace bindings {

    /// runtime is a behavior_runtime, copied. null takes it away
    void native_behavior_set_runtime(const void* runtime);
    /// the instance of the behaviour of that type on the object, made if it has none. null
    ///  when the object is gone or the type is not a behaviour
    void* native_behavior_add(int64_t script_object_id, const char* type_name);
    /// false when the object has no behaviour of that type
    nbool32 native_behavior_remove(int64_t script_object_id, const char* type_name);
    nbool32 native_behavior_set_enabled(int64_t script_object_id, const char* type_name, nbool32 enabled);

    /// has the script side hand its runtime over. called once the engine's functions are bound
    void attach_behavior_runtime();

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_BEHAVIOR_BINDINGS_HPP
