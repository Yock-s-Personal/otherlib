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

    /// for whoever draws an object's behaviours, by the scene object's id.
    ///  at: the behaviour's type name, whether it is switched on, and whether it was built
    ///  (one that was not keeps its name and its saved fields). empty for an index that is none
    int32_t native_behavior_count(natural_t object_id);
    const char* native_behavior_at(natural_t object_id, int32_t index, nbool32* out_enabled, nbool32* out_built);
    nbool32 native_behavior_set_enabled_on(natural_t object_id, const char* type_name, nbool32 enabled);
    /// what the behaviour's fields are, and what they hold now: each a field stream written
    ///  into out_bytes. the size it needs comes back, and nothing is written when that is
    ///  more than the capacity; 0 when there is nothing to say
    int32_t native_behavior_describe(natural_t object_id, const char* type_name, uint8_t* out_bytes, int32_t capacity);
    int32_t native_behavior_capture(natural_t object_id, const char* type_name, uint8_t* out_bytes, int32_t capacity);
    /// gives the behaviour the fields of the stream; the ones it does not name stay as they are
    void native_behavior_apply(natural_t object_id, const char* type_name, const uint8_t* bytes, int32_t size);

    /// has the script side hand its runtime over. called once the engine's functions are bound
    void attach_behavior_runtime();

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_BEHAVIOR_BINDINGS_HPP
