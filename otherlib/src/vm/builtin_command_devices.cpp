/**
 * \file vm/builtin_command_devices.cpp
 **/
#include "vm/builtin_command_devices.hpp"

#include "core/profiler.hpp"

#include "vm/command_bus.hpp"
#include "vm/devices/asset_device.hpp"
#include "vm/devices/core_command_device.hpp"
#include "vm/devices/event_device.hpp"
#include "vm/devices/project_device.hpp"
#include "vm/devices/scene_device.hpp"

namespace other {

  void register_builtin_command_devices(command_bus& bus) {
    PROFILE_SECTION("register_builtin_command_devices");
    bus.register_device(make_scope<core_command_device>());
    bus.register_device(make_scope<event_device>());
    bus.register_device(make_scope<asset_device>());
    bus.register_device(make_scope<scene_device>());
    bus.register_device(make_scope<project_device>());
  }

}  // namespace other
