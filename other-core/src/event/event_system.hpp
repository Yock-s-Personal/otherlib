/**
 * \file event/event_system.hpp
 **/
#ifndef OTHER_CORE_EVENT_EVENT_SYSTEM_HPP
#define OTHER_CORE_EVENT_EVENT_SYSTEM_HPP

#include <mutex>
#include <string_view>

#include "core/defines.hpp"
#include "core/fnv.hpp"
#include "core/scope.hpp"
#include "core/timer.hpp"
#include "core/value.hpp"
#include "event/event.hpp"

/// timers live behind timer_storage so this header stays asio-free
namespace asio {
  class io_context;
}

namespace other {

  class event_system {
   public:
    event_system(asio::io_context& io_ctx);
    virtual ~event_system();

    void clear();

    void trigger_event(const std::string_view name);
    void trigger_event(natural_t event_id);

    template <typename T>
    void trigger_event(const std::string_view name, const T& data) {
      set_user_data(FNV(name), data);
      trigger_event(name);
    }

    natural_t register_timed_event(const std::string_view name, microseconds duration, bool recurring = false);
    natural_t register_event(const std::string_view name);

    void cancel_event(const std::string_view name);
    void cancel_event(natural_t event_id);

    void set_user_data(natural_t event_id, const value& data);

    template <typename T>
    void set_user_data(const std::string_view name, const T& data) {
      if (name.empty()) {
        return;
      }
      if (!has_event(name)) {
        CORE_LOG_ERROR("Attempted to set user data for unregistered event name {}", name);
        return;
      }
      set_user_data(FNV(name), value{ data });
    }
    template <typename T>
    void set_user_data(natural_t event_id, const T& data) {
      if (!has_event(event_id)) {
        CORE_LOG_ERROR("Attempted to set user data for unregistered event ID {}", event_id);
        return;
      }
      set_user_data(event_id, value{ data });
    }

    /// what add_listener gives back: names one listener of one event. 0 is no listener
    using listener_id = natural_t;

    listener_id add_listener(const std::string_view name, event::handler callback);
    listener_id add_listener(natural_t id, event::handler callback);
    /// a listener already called for a trigger in progress still finishes that call
    void remove_listener(natural_t event_id, listener_id listener);

    void cancel_all();

    bool has_event(const std::string_view name) const;
    bool has_event(natural_t event_id) const;

    /// number of live timer entries; a recurring event holds exactly one for its lifetime
    natural_t active_timer_count() const;

   private:
    asio::io_context& io_context;

    mutable std::recursive_mutex events_mutex;

    struct listener_entry {
      listener_id id = 0;
      event::handler callback;
    };
    struct event_ctx {
      event ev;
      ostd::vector<listener_entry> listeners;
    };
    listener_id next_listener_id = 1;
    struct timer_storage;
    ostd::vector<event_ctx> registered_events;
    scope<timer_storage> timers;

    void post_event_callback(natural_t event_id, microseconds duration);
  };

}  // namespace other

#endif  // OTHER_CORE_EVENT_EVENT_SYSTEM_HPP