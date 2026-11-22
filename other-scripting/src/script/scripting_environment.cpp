/**
 * \file script/scripting_environment.cpp
 **/
#include "script/scripting_environment.hpp"

#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "dotnet/dotnet_object.hpp"
#include "script/script_object.hpp"

namespace other {
  namespace {

    /// what runs behaviours, once a script has handed it over
    behavior_runtime runtime_in_use;
    bool runtime_set = false;

    /// the name a script gives each callback, for what is said when one throws
    constexpr std::string_view kCallbackNames[] = {
      "OnAwake",
      "OnEnable",
      "OnStart",
      "OnFixedUpdate",
      "OnUpdate",
      "OnLateUpdate",
      "OnRender",
      "OnCollisionEnter",
      "OnCollisionExit",
      "OnTriggerEnter",
      "OnTriggerExit",
      "OnJointBreak",
      "OnDisable",
      "OnDestroy",
    };
    static_assert(std::size(kCallbackNames) == static_cast<size_t>(behavior_callback::NUM_CALLBACKS), "a name for every behaviour callback");

    /// what the runtime calls to turn an object id into an object: context is the resolver
    void* resolve_object(void* context, uint64_t object_id) {
      const auto* resolve = static_cast<const behavior_object_resolver*>(context);
      return (*resolve)(object_id);
    }

    /// what a capture is given to write into before its size is known
    constexpr size_t kFieldStreamGuess = 1024;

    /// a behavior's managed object is named after its parent, so a rename has to rename it
    std::string behavior_object_name(const std::string_view parent_name, const std::string_view behavior_name) {
      return std::format("{}__behavior__{}", parent_name, behavior_name);
    }

  }  // namespace

  void set_behavior_runtime(const behavior_runtime* runtime) {
    runtime_set = runtime != nullptr;
    runtime_in_use = runtime != nullptr ? *runtime : behavior_runtime{};
  }

  const behavior_runtime* get_behavior_runtime() {
    return runtime_set ? &runtime_in_use : nullptr;
  }

  void scripting_environment::initialize_script_environment(const config_table& configuration) {
    PROFILE_SECTION("scripting_environment::initialize-script-environment");
    script_object_pool = make_scope<memory_pool<script_object>>();
    OTHER_ASSERT(script_object_pool != nullptr, "Failed to create script object memory pool.");

    std::ranges::fill(live_objects, live_script_object{});

    dotnet.load_host(configuration);
    dotnet.call_entry_point();

    dotnet_load_context = dotnet.create_assembly_context("Other-DotNet-Assembly-Context");
    OTHER_ASSERT(dotnet_load_context != nullptr, "Failed to create assembly context for .NET assemblies.");

    lua.load_host(configuration);
    lua.call_entry_point();
  }

  void scripting_environment::destroy_all_objects() {
    PROFILE_SECTION("scripting_environment::destroy_all_objects");
    for (auto& obj : live_objects) {
      if (obj.object != nullptr) {
        destroy_object(obj.index);
      }
    }
  }

  void scripting_environment::shutdown_script_environment() {
    // clang-format off
    OTHER_ASSERT(std::ranges::all_of(live_objects, [](const live_script_object& obj) { return obj.status == live_script_object::DESTROYED; }), 
                 "Script leaks detected");
    // clang-format on

    PROFILE_SECTION("scripting_environment::shutdown_script_environment");
    {
      if (dotnet_load_context == nullptr) {
        CORE_LOG_ERROR("DotNet load context is not initialized.");
      } else {
        dotnet_load_context->unload_all();
        dotnet.destroy_assembly_context(dotnet_load_context->get_handle());
      }
      dotnet_load_context = nullptr;
    }
    dotnet.unload_host();

    OTHER_ASSERT(script_object_pool != nullptr, "Script object memory pool is not initialized.");
    script_object_pool->clear();
    script_object_pool = nullptr;
  }

  integer_t scripting_environment::create_object(const std::string_view name) {
    OTHER_ASSERT(script_object_pool != nullptr, "Script object memory pool is not initialized.");
    auto [obj, idx] = script_object_pool->emplace();

    auto& live_obj = live_objects[idx];
    OTHER_ASSERT(live_obj.object == nullptr, "Script object at index {} is already allocated.", idx);
    live_obj.index = idx;

    obj.id = idx;
    obj.name = name;
    live_obj.object = &obj;
    live_obj.status = live_script_object::LIVE;
    CORE_LOG_DEBUG(" - created script object [{}:{}] with ID {}", name, idx, idx);

    return idx;
  }

  void scripting_environment::destroy_object(integer_t id) {
    OTHER_ASSERT(script_object_pool != nullptr, "Script object memory pool is not initialized.");
    PROFILE_SECTION("scripting_environment::destroy_object");
    /// may occur if object encountered error during creation/on reload
    /// \todo maybe be stricter about this?
    if (id < 0 || id >= kMaxScriptObjects) {
      return;
    }
    /// stale handles (hot reload, behaviors already detached) may point at freed slots
    if (live_objects[id].status != live_script_object::LIVE) {
      return;
    }

    CORE_LOG_DEBUG(" - destroying script object with ID {}", id);

    script_object* obj = get_object(id);
    OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);

    /// behaviors go with their parent, so their managed objects' names are free for reuse
    for (size_t i = obj->behavior_handles.size(); i-- > 0;) {
      retire_behavior(obj, i);
    }
    obj->behavior_handles.clear();

    if (obj->dotnet_object != nullptr) {
      CORE_LOG_DEBUG(" - destroying .NET object for script object with ID {}", id);
      detach_dotnet_object(id);
    }

    script_object_pool->free(id);
    live_objects[id].object = nullptr;
    live_objects[id].status = live_script_object::DESTROYED;
  }

  void scripting_environment::dotnet_register_native_object(integer_t id, const std::string_view type_name) {
    PROFILE_SECTION("scripting_environment::dotnet_register_native_object");
    auto* obj = get_object(id);
    OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);

    if (obj->dotnet_object == nullptr) {
      CORE_LOG_ERROR("Script object with ID {} does not have a .NET object attached.", id);
      return;
    }

    native_string type_str = native_string::new_str(type_name);
    get_dotnet_host().interop().attach_native_object(id, obj->dotnet_object, type_str);
    native_string::free_str(type_str);
    obj->dotnet_native_registered = true;
  }

  void scripting_environment::dotnet_unregister_native_object(integer_t id) {
    PROFILE_SECTION("scripting_environment::dotnet_unregister_native_object");
    auto* obj = get_object(id);
    OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);

    if (obj->dotnet_object == nullptr) {
      CORE_LOG_ERROR("Script object with ID {} does not have a .NET object attached.", id);
      return;
    }

    get_dotnet_host().interop().detach_native_object(id, obj->dotnet_object);
    obj->dotnet_native_registered = false;
  }

  script_object* scripting_environment::get_object(integer_t id) {
    OTHER_ASSERT(script_object_pool != nullptr, "Script object memory pool is not initialized.");
    OTHER_ASSERT(id >= 0 && id < kMaxScriptObjects, "Invalid script object ID: {}", id);
    return script_object_pool->at(id);
  }

  void scripting_environment::reset_dotnet_object_binding(integer_t id) {
    script_object* obj = get_object(id);
    OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);
    if (obj->dotnet_object == nullptr) {
      return;
    }

    CORE_LOG_DEBUG("[script {}] resetting .NET native binding for [{}]", id, obj->name);
    obj->dotnet_object->invoke<>("ResetNativeHandle");
  }

  void scripting_environment::rename_object(integer_t id, const std::string_view name) {
    OTHER_ASSERT(!name.empty(), "Script object name cannot be empty.");
    PROFILE_SECTION("scripting_environment::rename_object");
    script_object* obj = get_object(id);
    OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);
    if (obj->name == name) {
      return;
    }

    CORE_LOG_DEBUG("[script {}] renaming [{}] to [{}]", id, obj->name, name);
    obj->name = name;
    if (obj->dotnet_object != nullptr) {
      obj->dotnet_object->object_name = obj->name;
    }

    for (const script_object::behavior_handle& handle : obj->behavior_handles) {
      if (handle.managed != nullptr) {
        handle.managed->object_name = behavior_object_name(obj->name, handle.type_name);
      }
    }
  }

  void scripting_environment::rebind_dotnet_object(integer_t id, void* native_handle) {
    script_object* obj = get_object(id);
    OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);
    if (obj->dotnet_object == nullptr) {
      return;
    }

    CORE_LOG_DEBUG("[script {}] rebinding .NET object [{}] to a new native handle", id, obj->name);
    obj->dotnet_object->invoke<>("RebindNativeHandle", native_handle);
  }

  ref<assembly> scripting_environment::load_dotnet_module(const std::string_view module_path) {
    OTHER_ASSERT(dotnet_load_context != nullptr, "DotNet load context is not initialized.");
    OTHER_ASSERT(!module_path.empty(), "Module path cannot be empty.");

    PROFILE_SECTION("scripting_environment::load-dotnet-module");
    if (!std::filesystem::exists(module_path)) {
      CORE_LOG_ERROR("Module file does not exist: {}", module_path);
      return nullptr;
    }

    ref<assembly> asm_ref = dotnet_load_context->load_assembly(module_path);
    if (asm_ref == nullptr) {
      CORE_LOG_ERROR("Failed to load assembly from path: {}", module_path);
    } else {
      CORE_LOG_DEBUG("Loaded assembly [{}:{}] from path: {}", asm_ref->get_handle(), asm_ref->get_name(), module_path);
    }

    return asm_ref;
  }

  ref<assembly> scripting_environment::get_dotnet_module(const std::string_view module_name) {
    OTHER_ASSERT(dotnet_load_context != nullptr, "DotNet load context is not initialized.");
    OTHER_ASSERT(!module_name.empty(), "Module name cannot be empty.");
    return dotnet_load_context->get_assembly_by_name(module_name);
  }

  ref<assembly> scripting_environment::get_dotnet_module_by_asset_path(const filepath& asset_path) {
    OTHER_ASSERT(dotnet_load_context != nullptr, "DotNet load context is not initialized.");
    OTHER_ASSERT(!asset_path.empty(), "Asset path cannot be empty.");
    return dotnet_load_context->get_assembly_by_id(FNV(asset_path.string()));
  }

  void scripting_environment::unload_dotnet_module(ref<assembly> module) {
    OTHER_ASSERT(dotnet_load_context != nullptr, "DotNet load context is not initialized.");
    PROFILE_SECTION("scripting_environment::unload_dotnet_module");
    dotnet_load_context->unload_assembly(module->get_handle());
    dotnet_load_context->remove_assembly(module->get_handle());

    if (dotnet_load_context->num_assemblies() == 0) {
      CORE_LOG_DEBUG("All assemblies unloaded from .NET context [{}:{}]", dotnet_load_context->get_handle(), dotnet_load_context->get_name());
      reset_dotnet_environment();
    }
  }

  void scripting_environment::reset_dotnet_environment() {
    PROFILE_SECTION("scripting_environment::reset_dotnet_environment");
    natural_t context_handle = dotnet_load_context ? dotnet_load_context->get_handle() : 0;
    dotnet_load_context = nullptr;
    dotnet.destroy_assembly_context(context_handle);

    dotnet_load_context = dotnet.create_assembly_context("Other-DotNet-Assembly-Context");
    OTHER_ASSERT(dotnet_load_context != nullptr, "Failed to create assembly context for .NET assemblies.");
  }

  bool scripting_environment::dotnet_object_has_attribute(integer_t id, const std::string_view attr_name) {
    script_object* obj = get_object(id);
    OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);
    if (obj->dotnet_object != nullptr) {
      return obj->dotnet_object->has_attribute(attr_name);
    } else {
      CORE_LOG_ERROR("Script object with ID {} does not have a .NET object attached.", id);
      return false;
    }
  }

  bool scripting_environment::dotnet_object_is_behavior(integer_t id) {
    script_object* obj = get_object(id);
    OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);
    if (obj->dotnet_object != nullptr) {
      return obj->dotnet_object->is_behavior();
    } else {
      CORE_LOG_ERROR("Script object with ID {} does not have a .NET object attached.", id);
      return false;
    }
  }

  void scripting_environment::attach_dotnet_behavior(integer_t parent_id, const std::string_view behavior_name) {
    PROFILE_SECTION("scripting_environment::attach_dotnet_behavior");

    script_object* parent = get_object(parent_id);
    OTHER_ASSERT(parent != nullptr, "Script object with ID {} does not exist.", parent_id);
    /// a parent can lack its managed object (type missing, constructor threw); the behavior is
    ///  dropped with an error instead of taking the process down mid-load
    if (parent->dotnet_object == nullptr) {
      CORE_LOG_ERROR("Cannot attach behavior '{}' to '{}': script object with ID {} has no .NET object attached.", behavior_name, parent->name, parent_id);
      return;
    }

    /// re-applying a scene document to a surviving object (play-stop restore) re-adds
    ///  behaviors that never detached - attach is idempotent per type
    auto existing = std::find_if(parent->behavior_handles.begin(), parent->behavior_handles.end(), [&behavior_name](const script_object::behavior_handle& handle) {
      return handle.type_name == behavior_name;
    });
    if (existing != parent->behavior_handles.end()) {
      CORE_LOG_DEBUG("[script {}] behavior '{}' already attached", parent_id, behavior_name);
      return;
    }

    CORE_LOG_DEBUG("[script {}] attaching behavior '{}' to object '{}'", parent_id, behavior_name, parent->name);

    /// kept without an instance when the type is missing (the project assembly failed to
    ///  build): a saved scene still names the behavior, and the next assembly refresh retries it
    script_object::behavior_handle handle = { .type_name = std::string(behavior_name) };
    const bool made = instantiate_dotnet_behavior(parent, handle);
    parent->behavior_handles.push_back(std::move(handle));
    if (!made) {
      return;
    }

    CORE_LOG_DEBUG("[script {}] behavior '{}' attached", parent_id, behavior_name);
    if (awake_deferrals > 0) {
      waiting_to_wake.push_back(parent_id);
    } else {
      wake_behavior(parent, parent->behavior_handles.size() - 1);
    }
  }

  bool scripting_environment::instantiate_dotnet_behavior(script_object* parent, script_object::behavior_handle& handle) {
    PROFILE_SECTION("scripting_environment::instantiate_dotnet_behavior");
    const behavior_runtime* runtime = get_behavior_runtime();
    if (runtime == nullptr) {
      CORE_LOG_ERROR("Behavior '{}' was not made for '{}': no script runtime has been handed to the engine.", handle.type_name, parent->name);
      return false;
    }

    dotnet_object* behavior_dotnet_obj = dotnet.instantiate_managed_object(handle.type_name, behavior_object_name(parent->name, handle.type_name));
    if (behavior_dotnet_obj == nullptr) {
      CORE_LOG_ERROR("Failed to instantiate .NET behavior object of type '{}' for '{}'", handle.type_name, parent->name);
      return false;
    }

    uint32_t callbacks = 0;
    if (!runtime->attach(parent->dotnet_object->managed_object, behavior_dotnet_obj->managed_object, handle.type_name.c_str(), &callbacks)) {
      CORE_LOG_ERROR("'{}' is not a behavior: it was not attached to '{}'.", handle.type_name, parent->name);
      dotnet.destroy_managed_object(behavior_dotnet_obj);
      return false;
    }

    handle.managed = behavior_dotnet_obj;
    handle.instance = behavior_dotnet_obj->managed_object;
    handle.callbacks = callbacks;
    handle.awake = false;
    handle.running = false;
    handle.started = false;
    /// a script reads its own copy of the switch
    runtime->set_enabled(handle.instance, handle.enabled);
    return true;
  }

  void scripting_environment::bind_object_to_runtime(script_object* obj) {
    const behavior_runtime* runtime = get_behavior_runtime();
    if (runtime != nullptr && obj->dotnet_object != nullptr) {
      runtime->bind_object(obj->dotnet_object->managed_object, obj->id);
    }
  }

  void scripting_environment::call_behavior(script_object* parent, size_t index, behavior_callback callback, const void* data) {
    const behavior_runtime* runtime = get_behavior_runtime();
    if (runtime == nullptr || index >= parent->behavior_handles.size()) {
      return;
    }

    void* const instance = parent->behavior_handles[index].instance;
    if (instance == nullptr || (parent->behavior_handles[index].callbacks & behavior_callback_bit(callback)) == 0) {
      return;
    }
    if (runtime->call(instance, static_cast<int32_t>(callback), data)) {
      return;
    }

    /// the script has logged what it threw. a behavior that throws is switched off, so
    ///  this is said once and not once a frame; found again because the list may have moved
    for (script_object::behavior_handle& handle : parent->behavior_handles) {
      if (handle.instance != instance) {
        continue;
      }
      CORE_LOG_ERROR("The behavior '{}' on '{}' threw in {} and is switched off. Set Enabled on it to run it again.", handle.type_name, parent->name, kCallbackNames[static_cast<size_t>(callback)]);
      handle.enabled = false;
      handle.running = false;
      runtime->set_enabled(instance, false);
    }
  }

  void scripting_environment::wake_behavior(script_object* parent, size_t index) {
    if (index >= parent->behavior_handles.size() || parent->behavior_handles[index].instance == nullptr || parent->behavior_handles[index].awake) {
      return;
    }

    void* const instance = parent->behavior_handles[index].instance;
    parent->behavior_handles[index].awake = true;
    call_behavior(parent, index, behavior_callback::AWAKE);

    /// one made while its scene plays begins running at once
    if (index < parent->behavior_handles.size() && parent->behavior_handles[index].instance == instance && parent->playing && parent->behavior_handles[index].enabled) {
      set_behavior_running(parent, index, true);
    }
  }

  void scripting_environment::set_behavior_running(script_object* parent, size_t index, bool running) {
    if (index >= parent->behavior_handles.size()) {
      return;
    }

    script_object::behavior_handle& handle = parent->behavior_handles[index];
    if (handle.instance == nullptr || !handle.awake || handle.running == running) {
      return;
    }

    if (running) {
      handle.running = true;
      handle.started = false;
      call_behavior(parent, index, behavior_callback::ENABLE);
      return;
    }

    void* const instance = handle.instance;
    call_behavior(parent, index, behavior_callback::DISABLE);
    if (index < parent->behavior_handles.size() && parent->behavior_handles[index].instance == instance) {
      parent->behavior_handles[index].running = false;
      parent->behavior_handles[index].started = false;
    }
  }

  void scripting_environment::retire_behavior(script_object* parent, size_t index) {
    if (index >= parent->behavior_handles.size() || parent->behavior_handles[index].instance == nullptr) {
      return;
    }

    void* const instance = parent->behavior_handles[index].instance;
    set_behavior_running(parent, index, false);
    if (index < parent->behavior_handles.size() && parent->behavior_handles[index].instance == instance && parent->behavior_handles[index].awake) {
      call_behavior(parent, index, behavior_callback::DESTROY);
    }
    if (index >= parent->behavior_handles.size() || parent->behavior_handles[index].instance != instance) {
      return;
    }

    script_object::behavior_handle& handle = parent->behavior_handles[index];
    if (const behavior_runtime* runtime = get_behavior_runtime(); runtime != nullptr && parent->dotnet_object != nullptr) {
      runtime->detach(parent->dotnet_object->managed_object, instance);
    }
    dotnet_object* const managed = handle.managed;
    handle.managed = nullptr;
    handle.instance = nullptr;
    handle.callbacks = 0;
    handle.awake = false;
    handle.running = false;
    handle.started = false;
    if (managed != nullptr) {
      dotnet.destroy_managed_object(managed);
    }
  }

  void scripting_environment::set_object_playing(integer_t id, bool playing) {
    PROFILE_SECTION("scripting_environment::set_object_playing");
    script_object* parent = get_object(id);
    if (parent == nullptr) {
      return;
    }

    parent->playing = playing;
    for (size_t i = 0; i < parent->behavior_handles.size(); ++i) {
      set_behavior_running(parent, i, playing && parent->behavior_handles[i].enabled);
    }
  }

  void scripting_environment::call_behaviors(integer_t id, behavior_callback callback, const void* data) {
    script_object* parent = get_object(id);
    if (parent == nullptr || parent->behavior_handles.empty()) {
      return;
    }
    PROFILE_SECTION("scripting_environment::call_behaviors");

    const bool to_enabled = callback == behavior_callback::RENDER;
    for (size_t i = 0; i < parent->behavior_handles.size(); ++i) {
      const script_object::behavior_handle& handle = parent->behavior_handles[i];
      if (handle.instance == nullptr || !handle.awake || (handle.callbacks & (behavior_callback_bit(callback) | behavior_callback_bit(behavior_callback::START))) == 0) {
        continue;
      }
      if (to_enabled) {
        if (handle.enabled) {
          call_behavior(parent, i, callback, data);
        }
        continue;
      }
      if (!handle.running) {
        continue;
      }

      if (!handle.started) {
        void* const instance = handle.instance;
        parent->behavior_handles[i].started = true;
        call_behavior(parent, i, behavior_callback::START);
        if (i >= parent->behavior_handles.size() || parent->behavior_handles[i].instance != instance || !parent->behavior_handles[i].running) {
          continue;
        }
      }
      call_behavior(parent, i, callback, data);
    }
  }

  bool scripting_environment::set_behavior_enabled(integer_t parent_id, const std::string_view behavior_name, bool enabled) {
    script_object* parent = get_object(parent_id);
    if (parent == nullptr) {
      return false;
    }

    for (size_t i = 0; i < parent->behavior_handles.size(); ++i) {
      script_object::behavior_handle& handle = parent->behavior_handles[i];
      if (handle.type_name != behavior_name) {
        continue;
      }

      handle.enabled = enabled;
      if (const behavior_runtime* runtime = get_behavior_runtime(); runtime != nullptr && handle.instance != nullptr) {
        runtime->set_enabled(handle.instance, enabled);
      }
      set_behavior_running(parent, i, enabled && parent->playing);
      return true;
    }
    return false;
  }

  void* scripting_environment::get_behavior_instance(integer_t parent_id, const std::string_view behavior_name) {
    script_object* parent = get_object(parent_id);
    if (parent == nullptr) {
      return nullptr;
    }

    const auto found = std::ranges::find(parent->behavior_handles, behavior_name, &script_object::behavior_handle::type_name);
    return found != parent->behavior_handles.end() ? found->instance : nullptr;
  }

  void scripting_environment::defer_behavior_awake(bool defer) {
    if (defer) {
      ++awake_deferrals;
      return;
    }

    OTHER_ASSERT(awake_deferrals > 0, "Behavior awake was not deferred.");
    if (--awake_deferrals > 0) {
      return;
    }

    /// taken first: a behavior that wakes may make others, which then wake at once
    const ostd::vector<integer_t> waiting = std::move(waiting_to_wake);
    waiting_to_wake.clear();
    for (const integer_t parent_id : waiting) {
      if (parent_id < 0 || static_cast<size_t>(parent_id) >= kMaxScriptObjects || live_objects[parent_id].status != live_script_object::LIVE) {
        continue;
      }
      script_object* parent = get_object(parent_id);
      for (size_t i = 0; parent != nullptr && i < parent->behavior_handles.size(); ++i) {
        wake_behavior(parent, i);
      }
    }
  }

  ostd::vector<uint8_t> scripting_environment::capture_fields(const script_object::behavior_handle& handle) {
    const behavior_runtime* runtime = get_behavior_runtime();
    if (runtime == nullptr || runtime->capture == nullptr || handle.instance == nullptr) {
      return handle.fields;
    }
    PROFILE_SECTION("scripting_environment::capture_fields");

    ostd::vector<uint8_t> stream(kFieldStreamGuess);
    int32_t size = runtime->capture(handle.instance, stream.data(), static_cast<int32_t>(stream.size()));
    if (size > static_cast<int32_t>(stream.size())) {
      stream.resize(static_cast<size_t>(size));
      size = runtime->capture(handle.instance, stream.data(), static_cast<int32_t>(stream.size()));
    }
    stream.resize(size > 0 && size <= static_cast<int32_t>(stream.size()) ? static_cast<size_t>(size) : 0);
    return stream;
  }

  void scripting_environment::apply_fields(const script_object::behavior_handle& handle, std::span<const uint8_t> stream, const behavior_object_resolver& resolve) {
    const behavior_runtime* runtime = get_behavior_runtime();
    if (runtime == nullptr || runtime->apply == nullptr || handle.instance == nullptr || stream.empty()) {
      return;
    }
    PROFILE_SECTION("scripting_environment::apply_fields");

    const behavior_object_resolver& used = resolve != nullptr ? resolve : object_resolver;
    if (used != nullptr) {
      runtime->apply(handle.instance, stream.data(), static_cast<int32_t>(stream.size()), &resolve_object, const_cast<behavior_object_resolver*>(&used));
    } else {
      runtime->apply(handle.instance, stream.data(), static_cast<int32_t>(stream.size()), nullptr, nullptr);
    }
  }

  ostd::vector<uint8_t> scripting_environment::capture_behavior_fields(integer_t parent_id, const std::string_view behavior_name) {
    script_object* parent = get_object(parent_id);
    if (parent == nullptr) {
      return {};
    }

    const auto found = std::ranges::find(parent->behavior_handles, behavior_name, &script_object::behavior_handle::type_name);
    return found != parent->behavior_handles.end() ? capture_fields(*found) : ostd::vector<uint8_t>{};
  }

  void scripting_environment::apply_behavior_fields(integer_t parent_id, const std::string_view behavior_name, std::span<const uint8_t> stream, const behavior_object_resolver& resolve) {
    script_object* parent = get_object(parent_id);
    if (parent == nullptr) {
      return;
    }

    const auto found = std::ranges::find(parent->behavior_handles, behavior_name, &script_object::behavior_handle::type_name);
    if (found == parent->behavior_handles.end()) {
      return;
    }
    /// no instance to give them to: kept, so a save writes them back and a rebuild that
    ///  brings the type applies them
    if (found->instance == nullptr) {
      found->fields.assign(stream.begin(), stream.end());
      return;
    }
    apply_fields(*found, stream, resolve);
  }

  const ostd::vector<behavior_field_description>& scripting_environment::describe_behavior(integer_t parent_id, const std::string_view behavior_name) {
    static const ostd::vector<behavior_field_description> kNone = {};
    script_object* parent = get_object(parent_id);
    const behavior_runtime* runtime = get_behavior_runtime();
    if (parent == nullptr || runtime == nullptr || runtime->describe == nullptr) {
      return kNone;
    }

    const auto handle = std::ranges::find(parent->behavior_handles, behavior_name, &script_object::behavior_handle::type_name);
    if (handle == parent->behavior_handles.end() || handle->instance == nullptr) {
      return kNone;
    }
    if (const auto known = descriptions.find(handle->type_name); known != descriptions.end()) {
      return known->second;
    }
    PROFILE_SECTION("scripting_environment::describe_behavior");

    ostd::vector<uint8_t> stream(kFieldStreamGuess);
    int32_t size = runtime->describe(handle->instance, stream.data(), static_cast<int32_t>(stream.size()));
    if (size > static_cast<int32_t>(stream.size())) {
      stream.resize(static_cast<size_t>(size));
      size = runtime->describe(handle->instance, stream.data(), static_cast<int32_t>(stream.size()));
    }
    stream.resize(size > 0 && size <= static_cast<int32_t>(stream.size()) ? static_cast<size_t>(size) : 0);

    ostd::vector<behavior_field_description>& described = descriptions[handle->type_name];
    if (!behavior_fields::read_description(stream, described)) {
      CORE_LOG_ERROR("What the fields of the behavior '{}' are could not be read.", handle->type_name);
    }
    return described;
  }

  void scripting_environment::set_behavior_object_resolver(behavior_object_resolver resolve) {
    object_resolver = std::move(resolve);
  }

  void* scripting_environment::get_object_instance(integer_t id) {
    if (id < 0 || static_cast<size_t>(id) >= kMaxScriptObjects || live_objects[id].status != live_script_object::LIVE) {
      return nullptr;
    }
    const script_object* obj = get_object(id);
    return obj != nullptr && obj->dotnet_object != nullptr ? obj->dotnet_object->managed_object : nullptr;
  }

  void scripting_environment::detach_dotnet_behavior(integer_t parent_id, const std::string_view behavior_name) {
    PROFILE_SECTION("scripting_environment::detach_dotnet_behavior");

    script_object* parent = get_object(parent_id);
    OTHER_ASSERT(parent != nullptr, "Script object with ID {} does not exist.", parent_id);

    CORE_LOG_DEBUG("[script {}] detaching behavior '{}' from object '{}'", parent_id, behavior_name, parent->name);

    auto it = std::find_if(parent->behavior_handles.begin(), parent->behavior_handles.end(), [&behavior_name](const script_object::behavior_handle& handle) {
      return handle.type_name == behavior_name;
    });
    if (it == parent->behavior_handles.end()) {
      CORE_LOG_WARN("Behavior '{}' not found on script object with ID {}", behavior_name, parent_id);
      return;
    }

    retire_behavior(parent, static_cast<size_t>(it - parent->behavior_handles.begin()));

    /// found again: what the behavior was told as it went may have moved the list
    it = std::find_if(parent->behavior_handles.begin(), parent->behavior_handles.end(), [&behavior_name](const script_object::behavior_handle& handle) {
      return handle.type_name == behavior_name;
    });
    if (it != parent->behavior_handles.end()) {
      parent->behavior_handles.erase(it);
    }
    CORE_LOG_DEBUG("[script {}] behavior '{}' detached", parent_id, behavior_name);
  }

  void scripting_environment::detach_all_dotnet_behaviors(integer_t parent_id) {
    PROFILE_SECTION("scripting_environment::detach_all_dotnet_behaviors");

    script_object* parent = get_object(parent_id);
    OTHER_ASSERT(parent != nullptr, "Script object with ID {} does not exist.", parent_id);

    CORE_LOG_DEBUG("[script {}] detaching all behaviors from object '{}'", parent_id, parent->name);
    for (size_t i = parent->behavior_handles.size(); i-- > 0;) {
      retire_behavior(parent, i);
    }
    parent->behavior_handles.clear();
  }

  void scripting_environment::keep_dotnet_behavior_fields(int32_t dotnet_type_id) {
    PROFILE_SECTION("scripting_environment::keep_dotnet_behavior_fields");
    const dotnet_type* kept_type = dotnet.get_type_cache()->get_type(dotnet_type_id);
    if (kept_type == nullptr) {
      return;
    }

    const std::string type_class_name = kept_type->class_name();
    const std::string type_full_name = kept_type->full_name();
    for (auto& live_obj : live_objects) {
      if (live_obj.status != live_script_object::LIVE || live_obj.object == nullptr) {
        continue;
      }
      for (script_object::behavior_handle& handle : live_obj.object->behavior_handles) {
        if (handle.instance != nullptr && (handle.type_name == type_class_name || handle.type_name == type_full_name)) {
          handle.fields = capture_fields(handle);
        }
      }
    }
  }

  void scripting_environment::invalidate_dotnet_script_objects_of_type(int32_t dotnet_type_id) {
    PROFILE_SECTION("scripting_environment::invalidate_dotnet_script_objects_of_type");
    CORE_LOG_DEBUG("Invalidating script objects with .NET type ID {}", dotnet_type_id);

    /// behaviors must be removed through the parent, or the native-side-only destroy roots
    ///  the unloading assembly; the handle stays invalid until reattach restores it
    if (dotnet_type* invalidated_type = dotnet.get_type_cache()->get_type(dotnet_type_id); invalidated_type != nullptr) {
      const std::string type_class_name = invalidated_type->class_name();
      const std::string type_full_name = invalidated_type->full_name();
      /// the next build's type may have other fields
      descriptions.erase(type_class_name);
      descriptions.erase(type_full_name);
      for (auto& live_obj : live_objects) {
        if (live_obj.status != live_script_object::LIVE || live_obj.object == nullptr) {
          continue;
        }

        script_object* parent = live_obj.object;
        if (parent->dotnet_object == nullptr) {
          continue;
        }

        for (size_t i = 0; i < parent->behavior_handles.size(); ++i) {
          const script_object::behavior_handle& handle = parent->behavior_handles[i];
          if (handle.instance == nullptr || (handle.type_name != type_class_name && handle.type_name != type_full_name)) {
            continue;
          }

          CORE_LOG_DEBUG(" - invalidating behavior '{}' on object '{}' due to matching .NET type ID {}", handle.type_name, parent->name, dotnet_type_id);
          retire_behavior(parent, i);
        }
      }
    }

    for (auto& live_obj : live_objects) {
      if (live_obj.status == live_script_object::LIVE && live_obj.object != nullptr) {
        script_object* obj = live_obj.object;
        if (obj->dotnet_object == nullptr || obj->dotnet_object->dn_type == nullptr) {
          continue;
        }

        if (obj->dotnet_object->dn_type->dotnet_id == dotnet_type_id) {
          CORE_LOG_DEBUG(" - invalidating script object [{}:{}] with ID {} due to matching .NET type ID {}", obj->name, obj->id, obj->id, dotnet_type_id);
          detach_dotnet_object(obj->id);
        }
      }
    }
  }

  void scripting_environment::reattach_invalidated_dotnet_behaviors() {
    PROFILE_SECTION("scripting_environment::reattach_invalidated_dotnet_behaviors");

    /// every instance is made before any is given its fields, since a field may hold
    ///  another object's behavior; and each has its fields before it is told it is awake
    struct remade_behavior {
      integer_t parent_id = -1;
      std::string type_name;
    };
    ostd::vector<remade_behavior> remade = {};
    for (auto& live_obj : live_objects) {
      if (live_obj.status != live_script_object::LIVE || live_obj.object == nullptr) {
        continue;
      }

      script_object* parent = live_obj.object;
      if (parent->dotnet_object == nullptr) {
        continue;
      }

      for (size_t i = 0; i < parent->behavior_handles.size(); ++i) {
        if (parent->behavior_handles[i].instance != nullptr) {
          continue;
        }

        CORE_LOG_DEBUG("[script {}] reattaching behavior '{}' to object '{}'", parent->id, parent->behavior_handles[i].type_name, parent->name);
        /// failure leaves the handle without an instance so the next assembly refresh retries
        ///  it (e.g. the behavior class was removed from this build of the assembly)
        if (instantiate_dotnet_behavior(parent, parent->behavior_handles[i])) {
          remade.push_back({ .parent_id = parent->id, .type_name = parent->behavior_handles[i].type_name });
        }
      }
    }

    for (const remade_behavior& made : remade) {
      script_object* parent = get_object(made.parent_id);
      const auto found = std::ranges::find(parent->behavior_handles, made.type_name, &script_object::behavior_handle::type_name);
      if (found != parent->behavior_handles.end() && found->instance != nullptr) {
        apply_fields(*found, found->fields, nullptr);
        found->fields.clear();
      }
    }

    for (const remade_behavior& made : remade) {
      if (live_objects[made.parent_id].status != live_script_object::LIVE) {
        continue;
      }
      script_object* parent = get_object(made.parent_id);
      const auto found = std::ranges::find(parent->behavior_handles, made.type_name, &script_object::behavior_handle::type_name);
      if (found != parent->behavior_handles.end()) {
        wake_behavior(parent, static_cast<size_t>(found - parent->behavior_handles.begin()));
      }
    }
  }

  void scripting_environment::detach_dotnet_object(integer_t id) {
    PROFILE_SECTION("scripting_environment::detach_dotnet_object");
    script_object* obj = get_object(id);
    OTHER_ASSERT(obj != nullptr, "Script object with ID {} does not exist.", id);

    if (obj->dotnet_object == nullptr) {
      CORE_LOG_ERROR("Script object with ID {} does not have a .NET object attached.", id);
      return;
    }

    CORE_LOG_DEBUG("[script {}] destroying .NET object [{}]", id, obj->name);

    /// behaviors were never attached to the NativeObjectManager; detaching them
    ///  there would throw C#-side
    if (obj->dotnet_native_registered) {
      dotnet_unregister_native_object(id);
    }
    dotnet.destroy_managed_object(obj->dotnet_object);
    obj->dotnet_object = nullptr;
  }

  lua_script* scripting_environment::load_lua_file(const std::string_view file_path) {
    return lua.load_file(file_path);
  }

}  // namespace other