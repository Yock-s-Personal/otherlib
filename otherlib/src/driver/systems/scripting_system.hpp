/**
 * \file driver/systems/scripting_system.hpp
 **/
#ifndef OTHERLIB_DRIVER_SYSTEMS_SCRIPTING_SYSTEM_HPP
#define OTHERLIB_DRIVER_SYSTEMS_SCRIPTING_SYSTEM_HPP

#include <filesystem>

#include "file/settled_changes.hpp"

#include "dotnet/dotnet_assembly.hpp"
#include "lua/lua_host.hpp"

#include "driver/systems/core_system.hpp"

namespace other {

  class lua_script;

  class OTHER_CLASS scripting_system : public core_system<scripting_system> {
   public:
    scripting_system(driver* driver_instance)
        : core_system(driver_instance, static_cast<uint32_t>(driver_system_type::SCRIPTING_DRIVER_SYSTEM)) {}

    std::string name() const override { return "Scripting System"; }

    void initialize(driver_kernel* kernel) override;
    void tick(driver_kernel* kernel, double dt) override;
    void shutdown(driver_kernel* kernel) override;

    /// null when the file is missing or is not an assembly
    ref<assembly> load_dotnet_module(const std::string_view module_path);
    /// takes back the named callbacks the module registered when it was loaded
    void unload_dotnet_module(ref<assembly> module_id);

    lua_host& get_lua_host();
    lua_script& get_envrc_script();

   private:
    // dotnet_object* dotnet_window_registry = nullptr;
    lua_script* driver_main_lua_script = nullptr;
    lua_script* envrc = nullptr;

    /// one of the driver's own assemblies, from scripting.dotnet-modules
    struct dotnet_module {
      std::string path;
      /// null while what is on disk could not be loaded
      ref<assembly> loaded = nullptr;
      /// the file as it was when it was last loaded: a different one is a new build
      uintmax_t size = 0;
      std::filesystem::file_time_type written;
      bool waiting_to_settle = false;
    };
    ostd::vector<dotnet_module> dotnet_modules;
    /// named callbacks by the handle of the module that registered them
    ostd::map<natural_t, ostd::vector<std::string>> module_callback_bindings;

    /// modules are looked at where assets are watched (the editor), and as often
    bool watch_modules = false;
    double watch_interval = 0.25;
    double watch_accumulator = 0.0;
    settled_changes changed_modules;

    void load_module(dotnet_module& module);
    void reload_changed_modules(double dt);

    /// callback names registered per SCRIPT_SOURCE asset so a refreshed/unloaded assembly
    ///   unregisters its callbacks before the fresh assembly registers them again
    ostd::map<natural_t, ostd::vector<std::string>> asset_callback_bindings;
  };

}  // namespace other

#endif  // OTHERLIB_DRIVER_SYSTEMS_SCRIPTING_SYSTEM_HPP