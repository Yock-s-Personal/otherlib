/**
 * \file driver/systems/scripting_system.cpp
 **/
#include "driver/systems/scripting_system.hpp"

#include "thread/thread_safety.hpp"

#include "lua/lua_script.hpp"
#include "script/scripting_environment.hpp"

#include "driver/driver.hpp"
#include "scripting/bindings.hpp"
#include "scripting/dotnet_bindings/event_bindings.hpp"
#include "scripting/dotnet_bindings/project_bindings.hpp"
#include "scripting/dotnet_bindings/window_bindings.hpp"
#include "tools/environment_console.hpp"

namespace other {

  void scripting_system::initialize(driver_kernel* kernel) {
    ASSERT_MAIN_THREAD();
    PROFILE_SECTION("scripting_system::initialize");
    driver_main_lua_script = subsystem<scripting_environment>::get()->load_lua_file("driver.lua");
    if (driver_main_lua_script) {
      environment_console::initialize(driver_main_lua_script);
    } else {
      CORE_LOG_ERROR("Failed to load driver main Lua script.");
    }

    get_driver().get_event_system()->register_event("console.check-command");
    get_driver().get_event_system()->add_listener("console.check-command", [this](const value& data) {
      if (data.type() == value_type::STRING) {
        std::string command = data;
        if (driver_main_lua_script && driver_main_lua_script->has_symbol("__is_command") && driver_main_lua_script->call_function<bool>("__is_command", command)) {
          get_driver().get_event_system()->trigger_event("console.command", command);
        } else {
          get_driver().get_event_system()->trigger_event("console.output", command);
        }
      }
    });

    /// load client specific .NET
    /// \note this has to happen here because .NET can override native subsystem implementations meaning we need to load these before initializing rendering or other subsystems
    ostd::vector<std::string> module_paths = get_driver().get_config_value<ostd::vector<std::string>>("scripting.dotnet-modules");
    for (const auto& module_path : module_paths) {
      CORE_LOG_DEBUG(" - .NET module to load: {}", module_path);
      load_module(dotnet_modules.emplace_back(dotnet_module{ .path = module_path }));
    }
    watch_modules = get_driver().get_config_value<bool>("filesystem.watch-assets", false);
    watch_interval = get_driver().get_config_value<double>("filesystem.watch-interval-ms", 250.0) / 1000.0;

    /// bind the native driver to the scripting environment to glue user scripts to the native environment
    do_script_interface_bindings(&get_driver());

    event_system& events = *get_driver().get_event_system();
    events.add_listener("script-source.asset-loaded", [this](const value& data) {
      OTHER_ASSERT(data.type() == value_type::UINT64, "Expected uint64 asset ID for script source asset-loaded event");
      PROFILE_SECTION("scripting_system::initialize--script-source-loaded-listener");
      uint64_t asset_id = data;

      asset* asset_ptr = get_driver().get_asset(asset_id);
      OTHER_ASSERT(asset_ptr != nullptr, "Asset pointer is null for asset ID: {}", asset_id);
      if (asset_ptr->asset_type != asset::type::SCRIPT_SOURCE) {
        CORE_LOG_ERROR("Received script-source.asset-loaded event for asset ID {} but asset type is not SCRIPT_SOURCE", asset_id);
        return;
      }

      auto* env = subsystem<scripting_environment>::get();
      OTHER_ASSERT(env != nullptr, "Scripting environment subsystem is not initialized.");

      ref<assembly> asm_ref = env->get_dotnet_module_by_asset_path(asset_ptr->absolute_path);
      OTHER_ASSERT(asm_ref != nullptr, "Failed to load .NET assembly for script asset: {}", asset_ptr->absolute_path.string());

      ostd::vector<callback_binding> bindings = asm_ref->get_native_function_bindings();
      CORE_LOG_DEBUG("Found {} native callback bindings in assembly [{}:{}]", bindings.size(), asm_ref->get_handle(), asm_ref->get_name());
      OTHER_ASSERT(!asset_callback_bindings.contains(asset_id), "SCRIPT_SOURCE asset {} loaded twice without an unload in between, named callbacks would collide.", asset_id);
      for (const auto& binding : bindings) {
        auto last_dot = binding.full_type_and_method_name.find_last_of('.');
        std::string type_name = binding.full_type_and_method_name.substr(0, last_dot);
        std::string method_name = binding.full_type_and_method_name.substr(last_dot + 1);
        CORE_LOG_DEBUG("Native Callback Binding registered: {} -> {}.{}", binding.binding_name, type_name, method_name);
        get_driver().get_interface_registry().register_named_callback(binding.binding_name, make_ref<dotnet_callback>(type_name, method_name));
        asset_callback_bindings[asset_id].push_back(binding.binding_name);
      }
    });

    /// fires for both the unload half of an assembly hot reload and a real unload (project
    ///   close), so refreshed assemblies re-register their callbacks against a clean registry
    events.add_listener("script-source.asset-unloaded", [this](const value& data) {
      OTHER_ASSERT(data.type() == value_type::UINT64, "Expected uint64 asset ID for script source asset-unloaded event");
      natural_t asset_id = data;

      auto itr = asset_callback_bindings.find(asset_id);
      if (itr == asset_callback_bindings.end()) {
        return;
      }

      for (const auto& binding_name : itr->second) {
        get_driver().get_interface_registry().unregister_named_callback(binding_name);
      }
      asset_callback_bindings.erase(itr);
    });
  }

  void scripting_system::tick(driver_kernel* kernel, double dt) {
    ASSERT_MAIN_THREAD();
    PROFILE_SECTION("scripting_system::tick");
    bindings::deliver_queued_script_events();
    bindings::begin_script_project_frame();
    reload_changed_modules(dt);
    if (environment_console::is_initialized()) {
      PROFILE_SECTION("scripting_system::tick--environment_console");
      environment_console::poll();
    }
  }

  void scripting_system::shutdown(driver_kernel* kernel) {
    ASSERT_MAIN_THREAD();
    PROFILE_SECTION("scripting_system::shutdown");

    do_script_interface_unbinding();
    for (dotnet_module& module : dotnet_modules) {
      if (module.loaded != nullptr) {
        unload_dotnet_module(module.loaded);
      }
    }
    dotnet_modules.clear();

    driver_main_lua_script = nullptr;
  }

  ref<assembly> scripting_system::load_dotnet_module(const std::string_view module_path) {
    ASSERT_MAIN_THREAD();
    PROFILE_SECTION("scripting_system::load_dotnet_module");
    CORE_LOG_DEBUG("Loading script module from path: {}", module_path);
    filepath path(module_path);
    if (!std::filesystem::exists(path)) {
      CORE_LOG_ERROR("Script module path does not exist: {}", module_path);
      return nullptr;
    }
    /// a file that is not an assembly yet (a build still writing it) is not loaded, and
    ///  the environment has said so
    ref<assembly> asm_ref = subsystem<scripting_environment>::get()->load_dotnet_module(path.string());
    if (asm_ref == nullptr) {
      return nullptr;
    }

    ostd::vector<callback_binding> bindings = asm_ref->get_native_function_bindings();
    CORE_LOG_DEBUG("Found {} native callback bindings in assembly [{}:{}]", bindings.size(), asm_ref->get_handle(), asm_ref->get_name());
    for (const auto& binding : bindings) {
      auto last_dot = binding.full_type_and_method_name.find_last_of('.');
      std::string type_name = binding.full_type_and_method_name.substr(0, last_dot);
      std::string method_name = binding.full_type_and_method_name.substr(last_dot + 1);
      CORE_LOG_DEBUG("Native Callback Binding registered: {} -> {}.{}", binding.binding_name, type_name, method_name);
      get_driver().get_interface_registry().register_named_callback(binding.binding_name, make_ref<dotnet_callback>(type_name, method_name));
      module_callback_bindings[asm_ref->get_handle()].push_back(binding.binding_name);
    }

    return asm_ref;
  }

  void scripting_system::unload_dotnet_module(ref<assembly> module) {
    ASSERT_MAIN_THREAD();
    PROFILE_SECTION("scripting_system::unload_dotnet_module");
    if (module == nullptr) {
      CORE_LOG_ERROR("Cannot unload a null module.");
      return;
    }

    CORE_LOG_DEBUG("Unloading script module with ID: {}", module->get_handle());
    if (const auto itr = module_callback_bindings.find(module->get_handle()); itr != module_callback_bindings.end()) {
      for (const std::string& binding_name : itr->second) {
        get_driver().get_interface_registry().unregister_named_callback(binding_name);
      }
      module_callback_bindings.erase(itr);
    }
    subsystem<scripting_environment>::get()->unload_dotnet_module(module);
  }

  void scripting_system::load_module(dotnet_module& module) {
    PROFILE_SECTION("scripting_system::load_module");
    /// read before the load: a build that lands during it is then seen as a change
    std::error_code ec;
    module.size = std::filesystem::file_size(module.path, ec);
    module.written = std::filesystem::last_write_time(module.path, ec);

    module.loaded = load_dotnet_module(module.path);
    if (module.loaded == nullptr) {
      CORE_LOG_ERROR("Failed to load .NET module: {}", module.path);
      return;
    }
    /// where a project declares a type of the same name, a scene means the project's
    module.loaded->driver_module = true;

    /// a driver's own modules may declare editor windows
    bindings::host_script_windows_of(module.loaded->dotnet_id);
  }

  void scripting_system::reload_changed_modules(double dt) {
    if (!watch_modules || dotnet_modules.empty()) {
      return;
    }
    watch_accumulator += dt;
    if (watch_accumulator < watch_interval) {
      return;
    }
    watch_accumulator = 0.0;
    PROFILE_SECTION("scripting_system::reload_changed_modules");

    for (dotnet_module& module : dotnet_modules) {
      if (module.waiting_to_settle) {
        continue;
      }

      /// a file a build is replacing may be missing for a moment: it is looked at again
      std::error_code ec;
      const uintmax_t size = std::filesystem::file_size(module.path, ec);
      if (ec) {
        continue;
      }
      const std::filesystem::file_time_type written = std::filesystem::last_write_time(module.path, ec);
      if (ec || (size == module.size && written == module.written)) {
        continue;
      }

      changed_modules.note(module.path);
      module.waiting_to_settle = true;
    }
    if (changed_modules.empty()) {
      return;
    }

    /// one interval without a change is what settled means
    const settled_changes::result changes = changed_modules.check();
    const auto module_of = [this](const filepath& path) -> dotnet_module* {
      const auto itr = std::ranges::find_if(dotnet_modules, [&path](const dotnet_module& module) { return filepath{ module.path } == path; });
      return itr == dotnet_modules.end() ? nullptr : &*itr;
    };

    for (const filepath& path : changes.gone) {
      if (dotnet_module* module = module_of(path); module != nullptr) {
        module->waiting_to_settle = false;
        CORE_LOG_WARN("The .NET module '{}' was removed and has not come back; what was loaded from it stays as it is", module->path);
      }
    }
    for (const filepath& path : changes.settled) {
      dotnet_module* module = module_of(path);
      if (module == nullptr) {
        continue;
      }

      /// unloading takes the module's windows, menu items and event subscriptions with it;
      ///  the windows that were open are open again once the new build is loaded
      CORE_LOG_INFO("The .NET module '{}' changed on disk: loading it again", module->path);
      module->waiting_to_settle = false;
      if (module->loaded != nullptr) {
        unload_dotnet_module(module->loaded);
        module->loaded = nullptr;
      }
      load_module(*module);
    }
  }

  lua_host& scripting_system::get_lua_host() {
    ASSERT_MAIN_THREAD();
    OTHER_ASSERT(!subsystem<scripting_environment>::inert, "Scripting environment subsystem is inert, cannot get Lua host.");
    return subsystem<scripting_environment>::get()->get_lua_host();
  }

  lua_script& scripting_system::get_envrc_script() {
    ASSERT_MAIN_THREAD();
    OTHER_ASSERT(envrc != nullptr, "Driver environment runtime script is not loaded.");
    return *envrc;
  }

}  // namespace other