/**
 * \file scripting/dotnet_bindings/event_bindings.cpp
 **/
#include "scripting/dotnet_bindings/event_bindings.hpp"

#include <mutex>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "core/logger.hpp"
#include "core/profiler.hpp"
#include "event/event_system.hpp"
#include "thread/thread_safety.hpp"

#include "script/scripting_environment.hpp"

#include "driver/driver.hpp"
#include "scripting/dotnet_bindings/driver_bindings.hpp"

namespace other {
  namespace bindings {

    namespace {

      struct subscription {
        event_system* events = nullptr;
        natural_t event_id = 0;
        event_system::listener_id listener = 0;
      };

      struct queued_event {
        natural_t subscription = 0;
        value data;
      };

      /// plain containers: these outlive the arena
      struct bridge_state {
        event_system* events_override = nullptr;
        /// main thread only
        std::unordered_map<natural_t, subscription> subscriptions;
        natural_t next_subscription = 1;

        std::mutex queue_mutex;
        std::vector<queued_event> queue;
      };

      bridge_state& bridge() {
        static bridge_state state;
        return state;
      }

      event_system* events() {
        if (bridge().events_override != nullptr) {
          return bridge().events_override;
        }
        driver* d = detail::get_dotnet_native_driver_unchecked();
        return d != nullptr ? d->get_event_system().get() : nullptr;
      }

      /// one call into the script side: the subscription, what its data is, and the bytes
      void dispatch(natural_t subscription_id, script_value_kind kind, const void* data, int32_t count) {
        if (subsystem<scripting_environment>::inert) {
          return;
        }
        auto* env = subsystem<scripting_environment>::get();
        if (env == nullptr) {
          return;
        }
        env->call_static_dotnet_method<int32_t>("Other.Core.Events", "Dispatch", subscription_id, static_cast<int32_t>(kind), data, count);
      }

      /// integers narrower than 32 bits are widened; a kind a script has no type for goes
      ///  as NONE, which still reaches handlers that take no data
      void deliver(natural_t subscription_id, const value& data) {
        PROFILE_SECTION("bindings::deliver-script-event");
        if (data.is_empty()) {
          dispatch(subscription_id, script_value_kind::NONE, nullptr, 0);
          return;
        }

        switch (data.type()) {
          case value_type::OEBOOL: {
            const uint32_t v = static_cast<const bool&>(data) ? 1u : 0u;
            dispatch(subscription_id, script_value_kind::BOOL, &v, 1);
          } break;

          case value_type::INT8: {
            const int32_t v = static_cast<const int8_t&>(data);
            dispatch(subscription_id, script_value_kind::INT32, &v, 1);
          } break;

          case value_type::INT16: {
            const int32_t v = static_cast<const int16_t&>(data);
            dispatch(subscription_id, script_value_kind::INT32, &v, 1);
          } break;

          case value_type::INT32: dispatch(subscription_id, script_value_kind::INT32, &static_cast<const int32_t&>(data), 1); break;
          case value_type::INT64: dispatch(subscription_id, script_value_kind::INT64, &static_cast<const int64_t&>(data), 1); break;

          case value_type::UINT8: {
            const uint32_t v = static_cast<const uint8_t&>(data);
            dispatch(subscription_id, script_value_kind::UINT32, &v, 1);
          } break;

          case value_type::UINT16: {
            const uint32_t v = static_cast<const uint16_t&>(data);
            dispatch(subscription_id, script_value_kind::UINT32, &v, 1);
          } break;

          case value_type::UINT32: dispatch(subscription_id, script_value_kind::UINT32, &static_cast<const uint32_t&>(data), 1); break;
          case value_type::UINT64: dispatch(subscription_id, script_value_kind::UINT64, &static_cast<const uint64_t&>(data), 1); break;
          case value_type::FLOAT: dispatch(subscription_id, script_value_kind::FLOAT, &static_cast<const float&>(data), 1); break;
          case value_type::DOUBLE: dispatch(subscription_id, script_value_kind::DOUBLE, &static_cast<const double&>(data), 1); break;

          case value_type::STRING: {
            const std::string text = data.as_string();
            dispatch(subscription_id, script_value_kind::TEXT, text.data(), static_cast<int32_t>(text.size()));
          } break;

          case value_type::VEC2: dispatch(subscription_id, script_value_kind::VEC2, &static_cast<const glm::vec2&>(data), 2); break;
          case value_type::VEC3: dispatch(subscription_id, script_value_kind::VEC3, &static_cast<const glm::vec3&>(data), 3); break;
          case value_type::VEC4: dispatch(subscription_id, script_value_kind::VEC4, &static_cast<const glm::vec4&>(data), 4); break;

          default: dispatch(subscription_id, script_value_kind::NONE, nullptr, 0); break;
        }
      }

      /// scripts are called from the main thread only. an event raised on another is kept,
      ///  with a copy of its data, until the main thread asks for what is waiting
      void on_event(natural_t subscription_id, const value& data) {
        if (is_on_main_thread()) {
          deliver(subscription_id, data);
          return;
        }
        std::scoped_lock lock(bridge().queue_mutex);
        bridge().queue.push_back({ .subscription = subscription_id, .data = data });
      }

    }  // namespace

    void native_event_register(const char* event_name) {
      if (event_system* ev = events(); ev != nullptr) {
        ev->register_event(event_name);
      }
    }

    void native_event_trigger(const char* event_name) {
      if (event_system* ev = events(); ev != nullptr) {
        ev->trigger_event(event_name);
      }
    }

    void native_event_trigger_with_string(const char* event_name, const char* data) {
      if (event_system* ev = events(); ev != nullptr) {
        ev->trigger_event(event_name, std::string{ data });
      }
    }

    void native_event_trigger_with_value(const char* event_name, int32_t kind, const void* data) {
      event_system* ev = events();
      if (ev == nullptr) {
        return;
      }
      if (data == nullptr) {
        ev->trigger_event(event_name);
        return;
      }

      switch (static_cast<script_value_kind>(kind)) {
        case script_value_kind::BOOL: ev->trigger_event(event_name, *static_cast<const uint32_t*>(data) != 0); break;
        case script_value_kind::INT32: ev->trigger_event(event_name, *static_cast<const int32_t*>(data)); break;
        case script_value_kind::UINT32: ev->trigger_event(event_name, *static_cast<const uint32_t*>(data)); break;
        case script_value_kind::INT64: ev->trigger_event(event_name, *static_cast<const int64_t*>(data)); break;
        case script_value_kind::UINT64: ev->trigger_event(event_name, *static_cast<const uint64_t*>(data)); break;
        case script_value_kind::FLOAT: ev->trigger_event(event_name, *static_cast<const float*>(data)); break;
        case script_value_kind::DOUBLE: ev->trigger_event(event_name, *static_cast<const double*>(data)); break;
        case script_value_kind::TEXT: ev->trigger_event(event_name, std::string{ static_cast<const char*>(data) }); break;
        case script_value_kind::VEC2: ev->trigger_event(event_name, *static_cast<const glm::vec2*>(data)); break;
        case script_value_kind::VEC3: ev->trigger_event(event_name, *static_cast<const glm::vec3*>(data)); break;
        case script_value_kind::VEC4: ev->trigger_event(event_name, *static_cast<const glm::vec4*>(data)); break;
        default: ev->trigger_event(event_name); break;
      }
    }

    natural_t native_event_subscribe(const char* event_name) {
      event_system* ev = events();
      if (ev == nullptr) {
        return 0;
      }

      const natural_t event_id = ev->register_event(event_name);
      const natural_t subscription_id = bridge().next_subscription++;
      const event_system::listener_id listener = ev->add_listener(event_id, [subscription_id](const value& data) {
        on_event(subscription_id, data);
      });
      if (listener == 0) {
        return 0;
      }

      bridge().subscriptions[subscription_id] = { .events = ev, .event_id = event_id, .listener = listener };
      return subscription_id;
    }

    void native_event_unsubscribe(natural_t subscription_id) {
      auto it = bridge().subscriptions.find(subscription_id);
      if (it == bridge().subscriptions.end()) {
        return;
      }
      it->second.events->remove_listener(it->second.event_id, it->second.listener);
      bridge().subscriptions.erase(it);
    }

    void release_script_subscriptions() {
      for (const auto& [id, sub] : bridge().subscriptions) {
        sub.events->remove_listener(sub.event_id, sub.listener);
      }
      bridge().subscriptions.clear();

      std::scoped_lock lock(bridge().queue_mutex);
      bridge().queue.clear();
    }

    natural_t script_subscription_count() {
      return bridge().subscriptions.size();
    }

    void set_script_event_system(event_system* events) {
      release_script_subscriptions();
      bridge().events_override = events;
    }

    void deliver_queued_script_events() {
      std::vector<queued_event> waiting;
      {
        std::scoped_lock lock(bridge().queue_mutex);
        waiting.swap(bridge().queue);
      }
      for (const queued_event& queued : waiting) {
        /// unsubscribed while it waited
        if (bridge().subscriptions.contains(queued.subscription)) {
          deliver(queued.subscription, queued.data);
        }
      }
    }

  }  // namespace bindings
}  // namespace other
