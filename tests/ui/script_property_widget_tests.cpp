/**
 * \file tests/ui/script_property_widget_tests.cpp
 *
 * the inspector's rows for an object's behaviours: what a type's fields are, as its
 * attributes say, and that drawing them leaves ImGui as it found it
 **/
#include <algorithm>
#include <cstring>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "script/behavior_fields.hpp"
#include "script/scripting_environment.hpp"

#include "object/script_component.hpp"
#include "scene/scene.hpp"

#include "ui/script/script_property_widget.hpp"

#include "headless_imgui.hpp"
#include "other_test.hpp"

namespace other {

  class script_property_widget_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    void SetUp() override {
      other_test::SetUp();
      env = subsystem<scripting_environment>::get();
      ASSERT_NE(env, nullptr);
      ASSERT_NE(get_behavior_runtime(), nullptr);
    }

    scripting_environment* env = nullptr;
  };

  TEST_F(script_property_widget_tests, a_types_fields_are_described_as_its_attributes_say) {
    scene s("Described Scene");
    scene_object& object = s.create_object("Described");
    script_component* script = s.get_component<script_component>(&object);
    script->add_behavior("DescribedBehavior");

    const ostd::vector<behavior_field_description>& described = env->describe_behavior(script->script_object_id, "DescribedBehavior");
    ASSERT_EQ(described.size(), 5u);

    const behavior_field_description& speed = described[0];
    EXPECT_EQ(speed.name, "speed");
    EXPECT_EQ(speed.kind, behavior_field_kind::FLOAT);
    EXPECT_EQ(speed.label, "Top Speed");
    EXPECT_EQ(speed.tooltip, "how fast it goes");
    EXPECT_TRUE(speed.has_range);
    EXPECT_EQ(speed.range_min, 0.f);
    EXPECT_EQ(speed.range_max, 10.f);
    EXPECT_TRUE(speed.group.empty());
    EXPECT_FALSE(speed.read_only);

    const behavior_field_description& tint = described[1];
    EXPECT_EQ(tint.kind, behavior_field_kind::VEC3);
    EXPECT_TRUE(tint.color);
    EXPECT_EQ(tint.group, "Looks");
    EXPECT_TRUE(tint.label.empty());

    /// a group lasts until a field names another
    const behavior_field_description& gait = described[2];
    EXPECT_EQ(gait.kind, behavior_field_kind::ENUM);
    EXPECT_EQ(gait.group, "Looks");
    ASSERT_EQ(gait.members.size(), 3u);
    EXPECT_EQ(gait.members[0], "Walk");
    EXPECT_EQ(gait.members[2], "Sprint");

    const behavior_field_description& count = described[3];
    EXPECT_TRUE(count.group.empty());
    EXPECT_TRUE(count.separator);
    EXPECT_TRUE(count.read_only);
    EXPECT_FALSE(count.has_range);

    const behavior_field_description& limits = described[4];
    EXPECT_EQ(limits.kind, behavior_field_kind::STRUCT);
    ASSERT_EQ(limits.fields.size(), 3u);
    EXPECT_EQ(limits.fields[0].name, "Min");
    EXPECT_EQ(limits.fields[2].kind, behavior_field_kind::STRUCT);
    ASSERT_EQ(limits.fields[2].fields.size(), 3u);
    EXPECT_EQ(limits.fields[2].fields[2].kind, behavior_field_kind::ASSET);
  }

  TEST_F(script_property_widget_tests, drawing_an_objects_behaviors_leaves_imgui_as_it_was) {
    scene s("Drawn Scene");
    scene_object& object = s.create_object("Drawn");
    scene_object& other_object = s.create_object("Named");
    script_component* script = s.get_component<script_component>(&object);
    script->add_behavior("FieldsBehavior");
    script->add_behavior("DescribedBehavior");
    script->add_behavior("NoSuchBehaviorType");
    env->call_static_dotnet_method<int32_t>("FieldsProbe", "SetAll");

    /// a field that names another object is drawn by that object's name
    behavior_fields::writer link = {};
    link.add_value<natural_t>("target", behavior_field_kind::OBJECT, other_object.id);
    env->apply_behavior_fields(script->script_object_id, "FieldsBehavior", link.take(), [this, &s](natural_t id) -> void* {
      const script_component* named = s.try_get_component<script_component>(id);
      return named != nullptr ? env->get_object_instance(named->script_object_id) : nullptr;
    });
    const ostd::vector<uint8_t> before_fields = env->capture_behavior_fields(script->script_object_id, "FieldsBehavior");

    headless_imgui imgui;
    for (int32_t frame = 0; frame < 3; ++frame) {
      imgui.frame([&]() {
        ImGui::Begin("inspector");
        ImGuiErrorRecoveryState before{};
        ImGui::ErrorRecoveryStoreState(&before);

        /// nobody clicks anything: nothing is changed
        EXPECT_FALSE(ui::draw_script_behaviors(*script, &s));

        ImGuiErrorRecoveryState after{};
        ImGui::ErrorRecoveryStoreState(&after);
        EXPECT_EQ(std::memcmp(&before, &after, sizeof(before)), 0) << "something the widget opened was left open";
        ImGui::End();
      });
    }

    EXPECT_EQ(env->capture_behavior_fields(script->script_object_id, "FieldsBehavior"), before_fields);
    EXPECT_EQ(env->call_static_dotnet_method<int32_t>("FieldsProbe", "CheckAll"), 0);
  }

}  // namespace other
