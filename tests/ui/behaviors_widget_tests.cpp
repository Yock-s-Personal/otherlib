/**
 * \file tests/ui/behaviors_widget_tests.cpp
 *
 * the inspector's rows for an object's behaviours, which C# draws from what the engine
 * says: what a type's fields are, as its attributes say; what the bindings hand a window;
 * and that drawing the rows leaves ImGui, and the fields, as they were
 **/
#include <algorithm>
#include <cstring>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "script/behavior_fields.hpp"
#include "script/scripting_environment.hpp"

#include "object/script_component.hpp"
#include "scene/scene.hpp"

#include "scripting/dotnet_bindings/behavior_bindings.hpp"
#include "scripting/scene_for_bindings.hpp"

#include "headless_imgui.hpp"
#include "other_test.hpp"

namespace other {

  class behaviors_widget_tests : public other_test {
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

  TEST_F(behaviors_widget_tests, a_types_fields_are_described_as_its_attributes_say) {
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

  TEST_F(behaviors_widget_tests, a_window_is_told_an_objects_behaviors_and_their_fields) {
    scene s("Asked Scene");
    scene_object& object = s.create_object("Asked");
    script_component* script = s.get_component<script_component>(&object);
    script->add_behavior("FieldsBehavior");
    script->add_behavior("NoSuchBehaviorType");
    detail::set_scene_for_bindings(&s);

    ASSERT_EQ(bindings::native_behavior_count(object.id), 2);
    nbool32 enabled = false;
    nbool32 built = false;
    EXPECT_STREQ(bindings::native_behavior_at(object.id, 0, &enabled, &built), "FieldsBehavior");
    EXPECT_TRUE(enabled);
    EXPECT_TRUE(built);
    /// a type that could not be made keeps its name
    EXPECT_STREQ(bindings::native_behavior_at(object.id, 1, &enabled, &built), "NoSuchBehaviorType");
    EXPECT_FALSE(built);
    EXPECT_STREQ(bindings::native_behavior_at(object.id, 2, &enabled, &built), "");
    EXPECT_EQ(bindings::native_behavior_count(4000), 0);

    EXPECT_TRUE(bindings::native_behavior_set_enabled_on(object.id, "FieldsBehavior", false));
    (void)bindings::native_behavior_at(object.id, 0, &enabled, &built);
    EXPECT_FALSE(enabled);
    EXPECT_FALSE(bindings::native_behavior_set_enabled_on(object.id, "NotOnIt", false));

    /// what is captured is what the environment captures; too small a buffer is told the size
    const ostd::vector<uint8_t> captured = env->capture_behavior_fields(script->script_object_id, "FieldsBehavior");
    ASSERT_FALSE(captured.empty());
    ostd::vector<uint8_t> given(captured.size() + 16);
    EXPECT_EQ(bindings::native_behavior_capture(object.id, "FieldsBehavior", given.data(), 4), static_cast<int32_t>(captured.size()));
    ASSERT_EQ(bindings::native_behavior_capture(object.id, "FieldsBehavior", given.data(), static_cast<int32_t>(given.size())), static_cast<int32_t>(captured.size()));
    given.resize(captured.size());
    EXPECT_EQ(given, captured);

    /// the description is the stream the runtime wrote. a field that holds a behaviour
    ///  says which, so that what is dropped on it can be checked
    ostd::vector<uint8_t> described(16384);
    const int32_t size = bindings::native_behavior_describe(object.id, "FieldsBehavior", described.data(), static_cast<int32_t>(described.size()));
    ASSERT_GT(size, 0);
    ASSERT_LE(size, static_cast<int32_t>(described.size()));
    described.resize(static_cast<size_t>(size));
    ostd::vector<behavior_fields::field> fields = {};
    ASSERT_TRUE(behavior_fields::read(described, fields));
    const auto friend_field = std::ranges::find(fields, std::string_view{ "friend" }, &behavior_fields::field::name);
    ASSERT_NE(friend_field, fields.end());
    ostd::vector<behavior_fields::field> said = {};
    ASSERT_TRUE(behavior_fields::read(friend_field->body, said));
    const auto holds = std::ranges::find(said, std::string_view{ "behavior" }, &behavior_fields::field::name);
    ASSERT_NE(holds, said.end());
    EXPECT_EQ(holds->text(), "FriendBehavior");
    EXPECT_EQ(bindings::native_behavior_describe(object.id, "NoSuchBehaviorType", described.data(), static_cast<int32_t>(described.size())), 0);

    /// a field given through the binding is the behaviour's
    behavior_fields::writer edit = {};
    edit.add_value<int32_t>("whole", behavior_field_kind::INT32, 41);
    const ostd::vector<uint8_t> stream = edit.take();
    bindings::native_behavior_apply(object.id, "FieldsBehavior", stream.data(), static_cast<int32_t>(stream.size()));
    ostd::vector<behavior_fields::field> after = {};
    const ostd::vector<uint8_t> changed = env->capture_behavior_fields(script->script_object_id, "FieldsBehavior");
    ASSERT_TRUE(behavior_fields::read(changed, after));
    const auto whole = std::ranges::find(after, std::string_view{ "whole" }, &behavior_fields::field::name);
    ASSERT_NE(whole, after.end());
    int32_t value = 0;
    EXPECT_TRUE(whole->value(value));
    EXPECT_EQ(value, 41);

    detail::set_scene_for_bindings(nullptr);
  }

  TEST_F(behaviors_widget_tests, drawing_an_objects_behaviors_leaves_imgui_and_the_fields_as_they_were) {
    scene s("Drawn Scene");
    scene_object& object = s.create_object("Drawn");
    scene_object& other_object = s.create_object("Named");
    script_component* script = s.get_component<script_component>(&object);
    script->add_behavior("FieldsBehavior");
    script->add_behavior("DescribedBehavior");
    script->add_behavior("NoSuchBehaviorType");
    env->call_static_dotnet_method<int32_t>("FieldsProbe", "SetAll");
    detail::set_scene_for_bindings(&s);

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
        EXPECT_EQ(env->call_static_dotnet_method<int32_t>("InspectorProbe", "DrawBehaviors"), 0);

        ImGuiErrorRecoveryState after{};
        ImGui::ErrorRecoveryStoreState(&after);
        EXPECT_EQ(std::memcmp(&before, &after, sizeof(before)), 0) << "something the widget opened was left open";
        ImGui::End();
      });
    }

    EXPECT_EQ(env->capture_behavior_fields(script->script_object_id, "FieldsBehavior"), before_fields);
    EXPECT_EQ(env->call_static_dotnet_method<int32_t>("FieldsProbe", "CheckAll"), 0);
    detail::set_scene_for_bindings(nullptr);
  }

  TEST_F(behaviors_widget_tests, the_inspectors_other_widgets_draw_and_change_nothing_by_themselves) {
    scene s("Widget Scene");
    s.create_object("Drawn");
    detail::set_scene_for_bindings(&s);
    EXPECT_EQ(env->call_static_dotnet_method<int32_t>("InspectorProbe", "ScriptsKind"), 1);

    headless_imgui imgui;
    for (int32_t frame = 0; frame < 3; ++frame) {
      imgui.frame([&]() {
        ImGui::Begin("inspector");
        ImGuiErrorRecoveryState before{};
        ImGui::ErrorRecoveryStoreState(&before);

        EXPECT_EQ(env->call_static_dotnet_method<int32_t>("InspectorProbe", "DrawWidgets"), 0);

        ImGuiErrorRecoveryState after{};
        ImGui::ErrorRecoveryStoreState(&after);
        EXPECT_EQ(std::memcmp(&before, &after, sizeof(before)), 0) << "something a widget opened was left open";
        ImGui::End();
      });
    }
    detail::set_scene_for_bindings(nullptr);
  }

}  // namespace other
