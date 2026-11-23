/**
 * \file script/scripting_environment.hpp
 **/
#ifndef OTHER_SCRIPTING_SCRIPT_SCRIPTING_ENVIRONMENT_HPP
#define OTHER_SCRIPTING_SCRIPT_SCRIPTING_ENVIRONMENT_HPP

#include "core/memory_pool.hpp"
#include "core/profiler.hpp"
#include "core/scope.hpp"
#include "core/subsystem.hpp"

#include "dotnet/dotnet_host.hpp"
#include "dotnet/dotnet_object.hpp"
#include "lua/lua_host.hpp"
#include "script/behavior.hpp"
#include "script/behavior_fields.hpp"
#include "script/script_object.hpp"

namespace other {

  class scripting_environment : public subsystem<scripting_environment> {
   public:
    scripting_environment() = default;
    virtual ~scripting_environment() = default;

    void initialize_script_environment(const config_table& configuration);

    void destroy_all_objects();
    void shutdown_script_environment();

    integer_t create_object(const std::string_view name);
    void destroy_object(integer_t id);
    /// names are labels, not identity: updates the object, its managed object, and its
    ///  behaviors' managed objects, which are named after it
    void rename_object(integer_t id, const std::string_view name);

    void dotnet_register_native_object(integer_t id, const std::string_view type_name);
    void dotnet_unregister_native_object(integer_t id);

    /// play-stop restore: managed instances survive the native scene rebuild - only their
    ///  native binding resets and re-establishes; no Awake/Remove fires
    void reset_dotnet_object_binding(integer_t id);
    void rebind_dotnet_object(integer_t id, void* native_handle);

    // template <typename T>
    // void register_native_object(string name, T&& value);

    script_object* get_object(integer_t id);

    /// DOTNET
    dotnet_host& get_dotnet_host() { return dotnet; }
    const dotnet_host& get_dotnet_host() const { return dotnet; }

    ref<assembly> load_dotnet_module(const std::string_view module_path);
    ref<assembly> get_dotnet_module(const std::string_view module_name);
    ref<assembly> get_dotnet_module_by_asset_path(const filepath& asset_path);
    void unload_dotnet_module(ref<assembly> module_id);
    void reset_dotnet_environment();

    bool dotnet_object_has_attribute(integer_t id, const std::string_view attr_name);
    bool dotnet_object_is_behavior(integer_t id);

    void attach_dotnet_behavior(integer_t parent_id, const std::string_view behavior_name);

    /// the object's scene started or stopped playing: its enabled behaviours start or stop
    ///  running, and are told
    void set_object_playing(integer_t id, bool playing);
    /// a callback that goes to every behaviour of an object that should hear it: the updates
    ///  and what physics reports to running ones, render to enabled ones. a behaviour is told
    ///  to start before the first thing it hears after it began running
    void call_behaviors(integer_t id, behavior_callback callback, const void* data = nullptr);
    /// false when the object has no behaviour of that name
    bool set_behavior_enabled(integer_t parent_id, const std::string_view behavior_name, bool enabled);
    /// the runtime's handle to that behaviour; null when the object has none or it could
    ///  not be made
    void* get_behavior_instance(integer_t parent_id, const std::string_view behavior_name);
    /// while set, a behaviour that is made is not told it is awake. clearing it tells every
    ///  one that waited, in the order they were made: a scene is loaded whole before any
    ///  script in it runs. calls nest
    void defer_behavior_awake(bool defer);

    /// a behaviour's saved fields as a stream (behavior_fields.hpp): asked of its instance,
    ///  or what is kept for it while it has none. empty when the object has no such behaviour
    ostd::vector<uint8_t> capture_behavior_fields(integer_t parent_id, const std::string_view behavior_name);
    /// the stream's values given to the behaviour's instance, or kept until it has one.
    ///  resolve turns an object id in the stream into that object; without one the
    ///  environment's own is used
    void apply_behavior_fields(integer_t parent_id, const std::string_view behavior_name, std::span<const uint8_t> stream, const behavior_object_resolver& resolve = nullptr);
    /// how an object id is turned into an object when nobody says otherwise: after a
    ///  rebuild of the scripts, when the fields a behaviour had are given to its new instance
    void set_behavior_object_resolver(behavior_object_resolver resolve);
    /// the runtime's handle to a script object's own managed object; null when it has none
    void* get_object_instance(integer_t id);
    /// what the saved fields of a behaviour's type are, for whoever draws them. asked of the
    ///  runtime once for a type and kept until the type's assembly goes; none while the
    ///  behaviour has no instance
    const ostd::vector<behavior_field_description>& describe_behavior(integer_t parent_id, const std::string_view behavior_name);
    /// the same as its runtime wrote it, for one that reads the stream itself. empty where
    ///  describe_behavior has nothing
    std::span<const uint8_t> describe_behavior_stream(integer_t parent_id, const std::string_view behavior_name);
    void detach_dotnet_behavior(integer_t parent_id, const std::string_view behavior_name);
    void detach_all_dotnet_behaviors(integer_t parent_id);
    /// before an assembly's types go: what each behaviour of this type holds is kept for
    ///  the instance the next build makes. asked for every type of the assembly before any
    ///  is invalidated, since a field may hold a behaviour of another of its types
    void keep_dotnet_behavior_fields(int32_t dotnet_type_id);
    void invalidate_dotnet_script_objects_of_type(int32_t dotnet_type_id);
    void reattach_invalidated_dotnet_behaviors();

    template <typename... Args>
    void attach_dotnet_object(integer_t id, const std::string_view type_name, Args&&... ctor_args) {
      PROFILE_SECTION("scripting_environment::attach_dotnet_object");
      script_object* obj = get_object(id);
      OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);

      // handle already attached object
      if (obj->dotnet_object != nullptr) {
        CORE_LOG_WARN("Script object with ID {} already has a .NET object attached. Detaching previous object.", id);
        detach_dotnet_object(id);
      }

      OTHER_ASSERT(obj->dotnet_object == nullptr, "Script object with ID {} already has a .NET object attached.", id);

      CORE_LOG_DEBUG("[script {}] creating .NET object [{} {}]", id, type_name, obj->name);
      obj->dotnet_object = dotnet.instantiate_managed_object(type_name, obj->name, std::forward<Args>(ctor_args)...);
      if (obj->dotnet_object != nullptr) {
        dotnet_register_native_object(id, type_name);
        obj->dotnet_object->load_fields();
        bind_object_to_runtime(obj);
      } else {
        CORE_LOG_ERROR("Failed to attach .NET object of type {} to script object with ID {}", type_name, id);
      }
    }

    template <typename R = void, typename... Args>
      requires std::is_same_v<R, void> || std::is_pointer_v<R> || std::is_trivial_v<R>
    R call_dotnet_method(integer_t id, const std::string_view function_name, Args&&... ctor_args) {
      script_object* obj = get_object(id);
      OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);
      if (obj->dotnet_object != nullptr) {
        return call_method_impl<R>(obj, function_name, std::forward<Args>(ctor_args)...);
      } else {
        CORE_LOG_ERROR("Script object with ID {} does not have a .NET object attached.", id);
        return default_return_value<R>();
      }
    }

    template <typename R = void, typename... Args>
    R call_static_dotnet_method(const std::string_view type_name, const std::string_view function_name, Args&&... ctor_args) {
      return dotnet.invoke_static<R>(type_name, function_name, std::forward<Args>(ctor_args)...);
    }

    template <typename FT>
      requires std::is_copy_constructible_v<FT>
    FT get_dotnet_field(integer_t id, const std::string_view field_name) {
      script_object* obj = get_object(id);
      OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);
      if (obj->dotnet_object != nullptr) {
        return obj->dotnet_object->get_field<FT>(field_name);
      } else {
        CORE_LOG_ERROR("Script object with ID {} does not have a .NET object attached.", id);
        return FT{};
      }
    }

    template <typename FT>
      requires std::is_copy_constructible_v<FT>
    void set_dotnet_field(integer_t id, const std::string_view field_name, const FT& value) {
      script_object* obj = get_object(id);
      OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);
      if (obj->dotnet_object != nullptr) {
        obj->dotnet_object->set_field(field_name, value);
      } else {
        CORE_LOG_ERROR("Script object with ID {} does not have a .NET object attached.", id);
      }
    }

    template <typename FT>
      requires std::is_copy_constructible_v<FT>
    FT get_dotnet_property(integer_t id, const std::string_view property_name) {
      script_object* obj = get_object(id);
      OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);
      if (obj->dotnet_object != nullptr) {
        return obj->dotnet_object->get_property<FT>(property_name);
      } else {
        CORE_LOG_ERROR("Script object with ID {} does not have a .NET object attached.", id);
        return FT{};
      }
    }

    template <typename T>
    T get_dotnet_attribute(integer_t id, const std::string_view attr_name, const std::string_view field_name) {
      script_object* obj = get_object(id);
      OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);
      if (obj->dotnet_object != nullptr) {
        return obj->dotnet_object->get_attribute<T>(attr_name, field_name);
      } else {
        CORE_LOG_ERROR("Script object with ID {} does not have a .NET object attached.", id);
        return T{};
      }
    }

    void detach_dotnet_object(integer_t id);
    /// END DOTNET

    /// LUA
    lua_host& get_lua_host() { return lua; }
    lua_script* load_lua_file(const std::string_view file_path);
    /// END LUA

    constexpr static inline size_t kMaxScriptObjects = memory_pool<script_object>::kMaxObjects;
    ref<assembly> dotnet_binding_assembly = nullptr;

   private:
    /// makes the behaviour's managed object and has it join its parent. false, with the
    ///  handle left without an instance, when the type is missing or is not a behaviour
    bool instantiate_dotnet_behavior(script_object* parent, script_object::behavior_handle& handle);
    void bind_object_to_runtime(script_object* obj);

    /// the calls below take the behaviour's place in its parent's list and not a reference:
    ///  a callback may add or remove behaviours, which moves the list
    ///
    /// one callback, when the behaviour's type has written it. one that throws is switched off
    void call_behavior(script_object* parent, size_t index, behavior_callback callback, const void* data = nullptr);
    void wake_behavior(script_object* parent, size_t index);
    void set_behavior_running(script_object* parent, size_t index, bool running);
    /// told it is stopping and going, taken from its parent and its managed object
    ///  destroyed. its entry stays, without an instance
    void retire_behavior(script_object* parent, size_t index);
    ostd::vector<uint8_t> capture_fields(const script_object::behavior_handle& handle);
    void apply_fields(const script_object::behavior_handle& handle, std::span<const uint8_t> stream, const behavior_object_resolver& resolve);

    behavior_object_resolver object_resolver = nullptr;
    /// by the name a behaviour's type is attached under
    ostd::map<std::string, ostd::vector<behavior_field_description>> descriptions;
    ostd::map<std::string, ostd::vector<uint8_t>> description_streams;

    uint32_t awake_deferrals = 0;
    ostd::vector<integer_t> waiting_to_wake;

    struct live_script_object {
      enum {
        LIVE,
        DESTROYED
      };
      int8_t status = DESTROYED;
      size_t index = 0;
      script_object* object = nullptr;
    };

    assembly_context* dotnet_load_context = nullptr;
    dotnet_host dotnet;

    lua_host lua;

    std::array<live_script_object, kMaxScriptObjects> live_objects = {};
    scope<memory_pool<script_object>> script_object_pool = nullptr;

    template <typename R, typename... Args>
      requires std::is_same_v<R, void>
    void call_method_impl(script_object* object, const std::string_view method_name, Args&&... args) {
      OTHER_ASSERT(object != nullptr, "Script object is null");
      object->dotnet_object->invoke<>(method_name, std::forward<Args>(args)...);
    }

    template <typename R, typename... Args>
      requires std::is_pointer_v<R> || std::is_trivial_v<R>
    R call_method_impl(script_object* object, const std::string_view method_name, Args&&... args) {
      OTHER_ASSERT(object != nullptr, "Script object is null");
      return object->dotnet_object->invoke<R>(method_name, std::forward<Args>(args)...);
    }

    template <typename R>
    R default_return_value() {
      if constexpr (std::is_same_v<R, void>) {
        return;
      } else if constexpr (std::is_pointer_v<R>) {
        return nullptr;
      } else {
        return R{};
      }
    }

    void invoke_static_method_with_args(const std::string_view type_name, const std::string_view method_name, const void** argv, const managed_type* arg_ts, size_t argc);
  };

}  // namespace other

OTHER_DEPENDENT_SUBSYSTEM(
  other::scripting_environment,
  subsystem_profile::kArena,
  subsystem_profile::kLogger,
  subsystem_profile::kFileSystem,
  subsystem_profile::kInputSystem,
  subsystem_profile::kTypeDatabase, );

#endif  // OTHER_SCRIPTING_SCRIPT_SCRIPTING_ENVIRONMENT_HPP