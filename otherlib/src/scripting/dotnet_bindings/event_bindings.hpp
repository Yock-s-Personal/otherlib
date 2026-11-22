/**
 * \file scripting/dotnet_bindings/event_bindings.hpp
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_EVENT_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_EVENT_BINDINGS_HPP

#include "core/defines.hpp"

namespace other {

  class event_system;

  namespace bindings {

    /// what an event's data is when it crosses to a script or comes from one. Events.Kind
    ///  in other-csharp/src/core/events.cs is the same list in the same order
    enum class script_value_kind : int32_t {
      NONE = 0,
      BOOL = 1,
      INT32 = 2,
      UINT32 = 3,
      INT64 = 4,
      UINT64 = 5,
      FLOAT = 6,
      DOUBLE = 7,
      TEXT = 8,
      VEC2 = 9,
      VEC3 = 10,
      VEC4 = 11,
    };

    void native_event_register(const char* event_name);
    void native_event_trigger(const char* event_name);
    void native_event_trigger_with_string(const char* event_name, const char* data);
    /// data points at one value of the kind; NONE triggers with what the event last held
    void native_event_trigger_with_value(const char* event_name, int32_t kind, const void* data);

    /// registers the event if nobody has, and listens to it for a script. 0 when there is
    ///  no event system to listen on
    natural_t native_event_subscribe(const char* event_name);
    void native_event_unsubscribe(natural_t subscription);

    /// the event system scripts trigger and listen on, in place of the driver's. null goes
    ///  back to the driver's. taking either away takes every subscription back first
    void set_script_event_system(event_system* events);
    void release_script_subscriptions();
    natural_t script_subscription_count();
    /// hands scripts the events that were raised on other threads since the last call
    void deliver_queued_script_events();

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_EVENT_BINDINGS_HPP