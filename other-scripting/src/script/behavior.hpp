/**
 * \file script/behavior.hpp
 * a behaviour is a script on an object. the engine keeps its life (when it wakes, runs,
 * stops and goes) and reaches the script itself only through a table of functions, so what
 * runs a behaviour need not be .NET
 **/
#ifndef OTHER_SCRIPTING_SCRIPT_BEHAVIOR_HPP
#define OTHER_SCRIPTING_SCRIPT_BEHAVIOR_HPP

#include <cstdint>
#include <functional>

#include "core/defines.hpp"

#include "dotnet/types.hpp"

namespace other {

  /// what the engine can ask a behaviour to do, in the order of a behaviour's life.
  ///  BehaviorCallback in other-csharp/src/core/behavior-host.cs is the same list in the
  ///  same order
  enum class behavior_callback : int32_t {
    AWAKE = 0,
    ENABLE,
    START,
    FIXED_UPDATE,
    UPDATE,
    LATE_UPDATE,
    RENDER,
    COLLISION_ENTER,
    COLLISION_EXIT,
    TRIGGER_ENTER,
    TRIGGER_EXIT,
    JOINT_BREAK,
    DISABLE,
    DESTROY,

    NUM_CALLBACKS,
  };

  constexpr uint32_t behavior_callback_bit(behavior_callback callback) {
    return 1u << static_cast<uint32_t>(callback);
  }

  /// what a collision or trigger callback is told
  struct behavior_contact {
    uint64_t other_object_id = 0;
    float point[3] = { 0.f, 0.f, 0.f };
    float normal[3] = { 0.f, 0.f, 0.f };
  };

  /// what runs behaviours of one kind. object and instance are that runtime's own handles
  struct behavior_runtime {
    /// tells an object which script object it is, so it can ask for behaviours itself
    void (*bind_object)(void* object, int64_t script_object_id) = nullptr;
    /// the instance joins its object, under the name the engine knows it by. false when it
    ///  is not a behaviour; callbacks gets one bit for each callback its type has written
    nbool32 (*attach)(void* object, void* instance, const char* type_name, uint32_t* out_callbacks) = nullptr;
    void (*detach)(void* object, void* instance) = nullptr;
    /// false when the callback threw. data is what the callback is told: a
    ///  behavior_contact, a force (float), or nothing
    nbool32 (*call)(void* instance, int32_t callback, const void* data) = nullptr;
    /// the instance's own copy of the switch, which is the one a script reads
    void (*set_enabled)(void* instance, nbool32 enabled) = nullptr;

    /// the instance's saved fields as a stream (behavior_fields.hpp). returns the stream's
    ///  size and writes it only when it fits in capacity
    int32_t (*capture)(void* instance, uint8_t* out_bytes, int32_t capacity) = nullptr;
    /// each field of the stream the instance still has is given its value; the others are
    ///  left. resolve gives the runtime's handle to a scene object from its id, null for
    ///  none; it may itself be null, and then a field that names an object is left
    void (*apply)(void* instance, const uint8_t* bytes, int32_t size, void* (*resolve)(void* context, uint64_t object_id), void* context) = nullptr;
    /// what the saved fields of the instance's type are, for whoever draws them, as a
    ///  stream of structs (behavior_field_description). sized as capture is
    int32_t (*describe)(void* instance, uint8_t* out_bytes, int32_t capacity) = nullptr;
  };

  /// the runtime's handle to a scene object from its id; null when there is no such object
  using behavior_object_resolver = std::function<void*(natural_t object_id)>;

  /// null takes the runtime away: behaviours are then kept by name and not run
  void set_behavior_runtime(const behavior_runtime* runtime);
  /// null while no script has handed one over
  const behavior_runtime* get_behavior_runtime();

}  // namespace other

#endif  // OTHER_SCRIPTING_SCRIPT_BEHAVIOR_HPP
