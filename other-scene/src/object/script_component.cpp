/**
 * \file object/script_component.cpp
 **/
#include "object/script_component.hpp"

#include "core/profiler.hpp"

#include "script/scripting_environment.hpp"

namespace other {

  namespace {

    scripting_environment& environment() {
      auto* env = subsystem<scripting_environment>::get();
      OTHER_ASSERT(env != nullptr, "Scripting environment is not initialized.");
      return *env;
    }

  }  // namespace

  void script_component::fixed_update(double delta_time) {
    OTHER_ASSERT(script_object_id >= 0, "Invalid script object ID: {}", script_object_id);
    environment().call_behaviors(script_object_id, behavior_callback::FIXED_UPDATE);
  }

  void script_component::dispatch_physics_event(behavior_callback callback, natural_t other_id, const glm::vec3& point, const glm::vec3& normal) {
    if (script_object_id < 0) {
      return;
    }
    const behavior_contact contact = {
      .other_object_id = other_id,
      .point = { point.x, point.y, point.z },
      .normal = { normal.x, normal.y, normal.z },
    };
    environment().call_behaviors(script_object_id, callback, &contact);
  }

  void script_component::dispatch_joint_break(float force) {
    if (script_object_id < 0) {
      return;
    }
    environment().call_behaviors(script_object_id, behavior_callback::JOINT_BREAK, &force);
  }

  void script_component::update(double delta_time) {
    OTHER_ASSERT(script_object_id >= 0, "Invalid script object ID: {}", script_object_id);
    environment().call_behaviors(script_object_id, behavior_callback::UPDATE);
  }

  void script_component::late_update(double delta_time) {
    OTHER_ASSERT(script_object_id >= 0, "Invalid script object ID: {}", script_object_id);
    environment().call_behaviors(script_object_id, behavior_callback::LATE_UPDATE);
  }

  void script_component::render_update(double delta_time) {
    OTHER_ASSERT(script_object_id >= 0, "Invalid script object ID: {}", script_object_id);
    environment().call_behaviors(script_object_id, behavior_callback::RENDER);
  }

  void script_component::scene_start() {
    OTHER_ASSERT(script_object_id >= 0, "Invalid script object ID: {}", script_object_id);
    environment().set_object_playing(script_object_id, true);
  }

  void script_component::scene_stop() {
    OTHER_ASSERT(script_object_id >= 0, "Invalid script object ID: {}", script_object_id);
    environment().set_object_playing(script_object_id, false);
  }

  void script_component::reset_dotnet_binding() {
    OTHER_ASSERT(script_object_id >= 0, "Invalid script object ID: {}", script_object_id);
    PROFILE_SECTION("script_component::reset_dotnet_binding");

    auto* env = subsystem<scripting_environment>::get();
    OTHER_ASSERT(env != nullptr, "Scripting environment is not initialized.");

    env->reset_dotnet_object_binding(script_object_id);
  }

  void script_component::add_behavior(const std::string_view behavior_type_name) {
    OTHER_ASSERT(script_object_id >= 0, "Invalid script object ID: {}", script_object_id);
    PROFILE_SECTION("script_component::add_behavior");

    auto* env = subsystem<scripting_environment>::get();
    OTHER_ASSERT(env != nullptr, "Scripting environment is not initialized.");

    env->attach_dotnet_behavior(script_object_id, behavior_type_name);
  }

  void script_component::remove_behavior(const std::string_view behavior_type_name) {
    OTHER_ASSERT(script_object_id >= 0, "Invalid script object ID: {}", script_object_id);
    PROFILE_SECTION("script_component::remove_behavior");

    auto* env = subsystem<scripting_environment>::get();
    OTHER_ASSERT(env != nullptr, "Scripting environment is not initialized.");

    env->detach_dotnet_behavior(script_object_id, behavior_type_name);
  }

  void script_component::remove_all_behaviors() {
    OTHER_ASSERT(script_object_id >= 0, "Invalid script object ID: {}", script_object_id);
    PROFILE_SECTION("script_component::remove_all_behaviors");

    auto* env = subsystem<scripting_environment>::get();
    OTHER_ASSERT(env != nullptr, "Scripting environment is not initialized.");

    /// destroys the behavior script_objects and clears the C# side behavior list
    env->detach_all_dotnet_behaviors(script_object_id);
  }

}  // namespace other