/**
 * \file tests/scripting/behavior_fields_tests.cpp
 *
 * what a behaviour's fields hold, kept: out to a stream and back, into a text scene and a
 * binary one, put back when play stops and carried into a rebuilt script
 **/
#include <algorithm>

#include "serialization/scene_serializer.hpp"

#include "script/behavior_fields.hpp"
#include "script/script_object.hpp"
#include "script/scripting_environment.hpp"

#include "object/script_component.hpp"
#include "scene/scene.hpp"

#include "other_test.hpp"

namespace other {

  using namespace serialization;

  class behavior_fields_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    void SetUp() override {
      other_test::SetUp();
      env = subsystem<scripting_environment>::get();
      ASSERT_NE(env, nullptr);
      ASSERT_NE(get_behavior_runtime(), nullptr);
    }

    int32_t probe(const std::string_view method) {
      return env->call_static_dotnet_method<int32_t>("FieldsProbe", method);
    }

    integer_t script_id_of(scene& s, natural_t object_id) {
      const script_component* script = s.try_get_component<script_component>(object_id);
      return script != nullptr ? script->script_object_id : -1;
    }

    /// an object with the probe's behaviour, and one after it with something for a field
    ///  of the first to hold
    void make_pair(scene& s) {
      scene_object& holder = s.create_object("Holder");
      scene_object& held = s.create_object("Held");
      s.get_component<script_component>(&holder)->add_behavior("FieldsBehavior");
      s.get_component<script_component>(&held)->add_behavior("FriendBehavior");
    }

    natural_t id_of(scene& s, const std::string_view name) {
      scene_object* object = s.find_object(name);
      return object != nullptr ? object->id : 0;
    }

    ostd::vector<uint8_t> fields_of(scene& s, const std::string_view object_name, const std::string_view behavior_name) {
      return env->capture_behavior_fields(script_id_of(s, id_of(s, object_name)), behavior_name);
    }

    /// as a scene gives a behaviour its fields: an id in the stream is an object of s
    void give(scene& s, const std::string_view object_name, const std::string_view behavior_name, const ostd::vector<uint8_t>& stream) {
      const behavior_object_resolver resolve = [this, &s](natural_t object_id) -> void* {
        return env->get_object_instance(script_id_of(s, object_id));
      };
      env->apply_behavior_fields(script_id_of(s, id_of(s, object_name)), behavior_name, stream, resolve);
    }

    /// the holder's fields name the object after it, and the behaviour on that object
    void point_holder_at_held(scene& s) {
      behavior_fields::writer number = {};
      number.add_value<int32_t>("number", behavior_field_kind::INT32, 42);
      give(s, "Held", "FriendBehavior", number.take());

      behavior_fields::writer links = {};
      links.add_value<natural_t>("target", behavior_field_kind::OBJECT, id_of(s, "Held"));
      links.add_value<natural_t>("friend", behavior_field_kind::OBJECT, id_of(s, "Held"));
      give(s, "Holder", "FieldsBehavior", links.take());
    }

    void expect_holder_points_at_held(scene& s) {
      EXPECT_EQ(probe("TargetId"), static_cast<int32_t>(id_of(s, "Held") & 0x7fffffff));
      EXPECT_EQ(probe("FriendNumber"), 42);
      EXPECT_EQ(probe("FriendIsOnTarget"), 1);
    }

    const behavior_fields::field* find(const ostd::vector<behavior_fields::field>& fields, const std::string_view name) {
      const auto found = std::ranges::find(fields, name, &behavior_fields::field::name);
      return found != fields.end() ? &*found : nullptr;
    }

    scripting_environment* env = nullptr;
  };

  TEST_F(behavior_fields_tests, a_stream_is_written_read_and_rewritten) {
    behavior_fields::writer inner = {};
    inner.add_value<natural_t>("owner", behavior_field_kind::OBJECT, 7);
    inner.add_text("skin", behavior_field_kind::ASSET, "textures/skin.png");

    behavior_fields::writer out = {};
    out.add_value<float>("speed", behavior_field_kind::FLOAT, 4.5f);
    out.add_text("label", behavior_field_kind::TEXT, "héro");
    out.add_value<natural_t>("target", behavior_field_kind::OBJECT, 3);
    out.add_value<natural_t>("nobody", behavior_field_kind::OBJECT, 0);
    out.add_value<natural_t>("stranger", behavior_field_kind::OBJECT, 99);
    out.add_text("icon", behavior_field_kind::ASSET, "textures/floor.png");
    out.add_text("empty", behavior_field_kind::ASSET, "");
    out.add("limits", behavior_field_kind::STRUCT, inner.take());
    EXPECT_EQ(out.count(), 8u);
    const ostd::vector<uint8_t> stream = out.take();
    EXPECT_EQ(out.count(), 0u);

    ostd::vector<behavior_fields::field> fields = {};
    ASSERT_TRUE(behavior_fields::read(stream, fields));
    ASSERT_EQ(fields.size(), 8u);
    EXPECT_EQ(fields[0].name, "speed");
    EXPECT_EQ(fields[0].kind, behavior_field_kind::FLOAT);
    float speed = 0.f;
    EXPECT_TRUE(fields[0].value(speed));
    EXPECT_EQ(speed, 4.5f);
    double wrong_size = 0.0;
    EXPECT_FALSE(fields[0].value(wrong_size));
    EXPECT_EQ(fields[1].text(), "héro");

    /// ids go through the map, in a struct too; one the map lacks is no object, none stays none
    const ostd::map<natural_t, natural_t> ids = { { 3, 30 }, { 7, 70 } };
    const ostd::vector<uint8_t> remapped = behavior_fields::remap_objects(stream, ids);
    ASSERT_TRUE(behavior_fields::read(remapped, fields));
    ASSERT_EQ(fields.size(), 8u);
    natural_t id = 1;
    EXPECT_TRUE(find(fields, "target")->value(id));
    EXPECT_EQ(id, 30u);
    EXPECT_TRUE(find(fields, "nobody")->value(id));
    EXPECT_EQ(id, 0u);
    EXPECT_TRUE(find(fields, "stranger")->value(id));
    EXPECT_EQ(id, 0u);
    ostd::vector<behavior_fields::field> nested = {};
    ASSERT_TRUE(behavior_fields::read(find(fields, "limits")->body, nested));
    EXPECT_TRUE(find(nested, "owner")->value(id));
    EXPECT_EQ(id, 70u);
    EXPECT_EQ(find(fields, "label")->text(), "héro");

    ostd::vector<std::string> paths = {};
    behavior_fields::collect_assets(stream, paths);
    ASSERT_EQ(paths.size(), 2u);
    EXPECT_EQ(paths[0], "textures/floor.png");
    EXPECT_EQ(paths[1], "textures/skin.png");

    const ostd::vector<uint8_t> moved = behavior_fields::rewrite_assets(stream, [](const std::string& path) -> std::string {
      return "packed/" + path;
    });
    paths.clear();
    behavior_fields::collect_assets(moved, paths);
    ASSERT_EQ(paths.size(), 2u);
    EXPECT_EQ(paths[0], "packed/textures/floor.png");
    EXPECT_EQ(paths[1], "packed/textures/skin.png");

    /// a stream cut short is said to be, and is handed back as it is
    const std::span<const uint8_t> cut{ stream.data(), stream.size() - 3 };
    EXPECT_FALSE(behavior_fields::read(cut, fields));
    EXPECT_TRUE(fields.empty());
    EXPECT_EQ(behavior_fields::remap_objects(cut, ids).size(), cut.size());
    EXPECT_TRUE(behavior_fields::read({}, fields));
  }

  TEST_F(behavior_fields_tests, every_kind_goes_out_and_comes_back) {
    scene s("Fields Scene");
    make_pair(s);
    ASSERT_EQ(probe("CheckDefaults"), 0);

    probe("SetAll");
    probe("SetUnsaved");
    point_holder_at_held(s);
    const ostd::vector<uint8_t> stream = fields_of(s, "Holder", "FieldsBehavior");

    /// what is saved: the public fields and the marked private one, and nothing else
    ostd::vector<behavior_fields::field> fields = {};
    ASSERT_TRUE(behavior_fields::read(stream, fields));
    EXPECT_EQ(fields.size(), 23u);
    for (const std::string_view unsaved : { "scratch", "shared", "fixed_value", "Property", "never", "list" }) {
      EXPECT_EQ(find(fields, unsaved), nullptr) << unsaved << " is not a saved field";
    }
    ASSERT_NE(find(fields, "hidden"), nullptr);
    EXPECT_EQ(find(fields, "tiny")->kind, behavior_field_kind::INT32);
    EXPECT_EQ(find(fields, "usmall")->kind, behavior_field_kind::UINT32);
    EXPECT_EQ(find(fields, "gait")->kind, behavior_field_kind::ENUM);
    EXPECT_EQ(find(fields, "gait")->text(), "Sprint");
    EXPECT_EQ(find(fields, "letter")->text(), "Z");
    EXPECT_EQ(find(fields, "limits")->kind, behavior_field_kind::STRUCT);
    natural_t target = 0;
    EXPECT_TRUE(find(fields, "target")->value(target));
    EXPECT_EQ(target, id_of(s, "Held"));
    EXPECT_TRUE(find(fields, "friend")->value(target));
    EXPECT_EQ(target, id_of(s, "Held"));

    /// into another object's behaviour of the same type, which has the class's values
    scene_object& other_holder = s.create_object("Other Holder");
    s.get_component<script_component>(&other_holder)->add_behavior("FieldsBehavior");
    ASSERT_EQ(probe("CheckDefaults"), 0);
    give(s, "Other Holder", "FieldsBehavior", stream);
    EXPECT_EQ(probe("CheckAll"), 0);
    expect_holder_points_at_held(s);
    EXPECT_EQ(probe("CheckUnsavedAsMade"), 0);
  }

  TEST_F(behavior_fields_tests, a_text_scene_and_a_binary_one_hold_the_same_values) {
    scene source("Source Scene");
    make_pair(source);
    probe("SetAll");
    point_holder_at_held(source);
    const scene_document doc = capture_scene(source, default_codec_services());
    EXPECT_EQ(doc.schema_version, 2u);

    const std::string text = write_scene_toml(doc);
    for (const std::string_view expected : {
           "schema-version = 2",
           "[objects.components.script.fields.FieldsBehavior]",
           "speed = 0.1\n",
           "gait = \"Sprint\"",
           "spawn = [ 0.0, 1.5, -3.0 ]",
           "icon = { asset = \"textures/floor.png\" }",
           "target = { object = ",
           "[objects.components.script.fields.FieldsBehavior.limits]",
           "[objects.components.script.fields.FieldsBehavior.limits.Inner]",
           "[objects.components.script.fields.FriendBehavior]",
           "number = 42",
         }) {
      EXPECT_NE(text.find(expected), std::string::npos) << "the text scene lacks: " << expected << "\n"
                                                        << text;
    }

    const scene_parse_result from_text = parse_scene_toml(text);
    ASSERT_TRUE(from_text.success()) << from_text.error;
    EXPECT_TRUE(from_text.warnings.empty());
    {
      scene loaded("From Text");
      instantiate_scene(loaded, *from_text.document, default_codec_services());
      EXPECT_EQ(probe("CheckAll"), 0);
      expect_holder_points_at_held(loaded);
    }

    const scene_parse_result from_binary = parse_scene_binary(write_scene_binary(doc));
    ASSERT_TRUE(from_binary.success()) << from_binary.error;
    {
      scene loaded("From Binary");
      instantiate_scene(loaded, *from_binary.document, default_codec_services());
      EXPECT_EQ(probe("CheckAll"), 0);
      expect_holder_points_at_held(loaded);
    }

    /// a text scene written from what a text scene gave is that text again
    const std::string again = write_scene_toml(*from_text.document);
    const scene_parse_result from_again = parse_scene_toml(again);
    ASSERT_TRUE(from_again.success()) << from_again.error;
    EXPECT_EQ(write_scene_toml(*from_again.document), again);
  }

  TEST_F(behavior_fields_tests, an_object_named_before_it_exists_is_found) {
    /// the holder's record comes first and names the object made after it
    const std::string text = R"(
[scene]
schema-version = 2
name = "Order"

[[objects]]
id = 5
name = "Holder"
[objects.components.script]
behaviors = [ "FieldsBehavior" ]
[objects.components.script.fields.FieldsBehavior]
target = { object = 9 }
friend = { object = 9 }
whole = 8

[[objects]]
id = 9
name = "Held"
[objects.components.script]
behaviors = [ "FriendBehavior" ]
[objects.components.script.fields.FriendBehavior]
number = 42
)";
    const scene_parse_result parsed = parse_scene_toml(text);
    ASSERT_TRUE(parsed.success()) << parsed.error;

    scene loaded("Order");
    instantiate_scene(loaded, *parsed.document, default_codec_services());
    expect_holder_points_at_held(loaded);
    EXPECT_EQ(probe("Whole"), 8);
  }

  TEST_F(behavior_fields_tests, a_scene_from_before_fields_loads_with_the_scripts_values) {
    const std::string text = R"(
[scene]
schema-version = 1
name = "Old"

[[objects]]
id = 1
name = "Holder"
[objects.components.script]
behaviors = [ "FieldsBehavior" ]
)";
    const scene_parse_result parsed = parse_scene_toml(text);
    ASSERT_TRUE(parsed.success()) << parsed.error;
    EXPECT_TRUE(parsed.warnings.empty());

    scene loaded("Old");
    instantiate_scene(loaded, *parsed.document, default_codec_services());
    EXPECT_EQ(probe("CheckDefaults"), 0);
    const script_object* holder = env->get_object(script_id_of(loaded, id_of(loaded, "Holder")));
    ASSERT_EQ(holder->behavior_handles.size(), 1u);
    EXPECT_TRUE(holder->behavior_handles[0].enabled);
  }

  TEST_F(behavior_fields_tests, a_field_that_is_gone_changed_or_new_is_handled) {
    scene s("Changed Scene");
    make_pair(s);

    behavior_fields::writer limits = {};
    limits.add_value<double>("Max", behavior_field_kind::DOUBLE, 8.0);
    limits.add_value<float>("Soft", behavior_field_kind::FLOAT, 0.9f);

    behavior_fields::writer saved = {};
    /// no such field now
    saved.add_value<uint8_t>("retired", behavior_field_kind::BOOL, 1);
    /// was text, is a number now
    saved.add_text("speed", behavior_field_kind::TEXT, "fast");
    /// a number of another kind is a number
    saved.add_value<double>("whole", behavior_field_kind::DOUBLE, 8.0);
    /// one that does not fit where it is to go
    saved.add_value<int64_t>("tiny", behavior_field_kind::INT64, 4000);
    /// a member's name as text, which is how a text scene has it
    saved.add_text("gait", behavior_field_kind::TEXT, "Run");
    saved.add("limits", behavior_field_kind::STRUCT, limits.take());
    give(s, "Holder", "FieldsBehavior", saved.take());

    EXPECT_EQ(probe("SpeedTimesTen"), 15);
    EXPECT_EQ(probe("Whole"), 8);
    EXPECT_EQ(probe("Tiny"), -1);
    EXPECT_EQ(probe("Gait"), 1);
    EXPECT_EQ(probe("LimitsMaxTimesTen"), 80);

    /// a member the enum does not have leaves what was there
    behavior_fields::writer renamed = {};
    renamed.add_text("gait", behavior_field_kind::ENUM, "Gallop");
    give(s, "Holder", "FieldsBehavior", renamed.take());
    EXPECT_EQ(probe("Gait"), 1);
  }

  TEST_F(behavior_fields_tests, stop_puts_back_what_play_changed) {
    scene s("Play Scene");
    make_pair(s);
    probe("SetAll");
    point_holder_at_held(s);

    s.play();
    probe("ChangeWhilePlaying");
    ASSERT_EQ(probe("Whole"), 12345);
    ASSERT_TRUE(env->set_behavior_enabled(script_id_of(s, id_of(s, "Holder")), "FieldsBehavior", false));
    ASSERT_EQ(probe("IsEnabled"), 0);
    s.stop();

    /// the same instance, with what it had before play; and the object it names is the
    ///  one the restore made, which has another id
    EXPECT_EQ(probe("CheckAll"), 0);
    expect_holder_points_at_held(s);
    EXPECT_EQ(probe("IsEnabled"), 1);
  }

  TEST_F(behavior_fields_tests, a_behavior_switched_off_is_saved_so) {
    scene source("Switch Scene");
    make_pair(source);
    ASSERT_TRUE(env->set_behavior_enabled(script_id_of(source, id_of(source, "Holder")), "FieldsBehavior", false));
    const scene_document doc = capture_scene(source, default_codec_services());

    const std::string text = write_scene_toml(doc);
    EXPECT_NE(text.find("disabled = [ \"FieldsBehavior\" ]"), std::string::npos) << text;

    const scene_parse_result parsed = parse_scene_toml(text);
    ASSERT_TRUE(parsed.success()) << parsed.error;
    scene loaded("Switch Scene Again");
    instantiate_scene(loaded, *parsed.document, default_codec_services());
    EXPECT_EQ(probe("IsEnabled"), 0);
    const script_object* holder = env->get_object(script_id_of(loaded, id_of(loaded, "Holder")));
    ASSERT_EQ(holder->behavior_handles.size(), 1u);
    EXPECT_FALSE(holder->behavior_handles[0].enabled);

    /// off: play does not start it
    loaded.play();
    EXPECT_FALSE(holder->behavior_handles[0].running);
    loaded.stop();
  }

  TEST_F(behavior_fields_tests, an_asset_a_script_holds_is_collected_and_rewritten) {
    scene_document doc = {};
    {
      scene source("Asset Scene");
      make_pair(source);
      probe("SetAll");
      doc = capture_scene(source, default_codec_services());
    }

    const ostd::vector<component_asset_ref> refs = collect_scene_asset_refs(doc);
    EXPECT_TRUE(std::ranges::contains(refs, "textures/floor.png", &component_asset_ref::path));
    EXPECT_TRUE(std::ranges::contains(refs, "textures/skin.png", &component_asset_ref::path));

    const size_t changed = rewrite_scene_asset_refs(doc, [](const std::string& path) -> std::string {
      return "packed/" + path;
    });
    EXPECT_EQ(changed, 2u);
    const ostd::vector<component_asset_ref> moved = collect_scene_asset_refs(doc);
    EXPECT_TRUE(std::ranges::contains(moved, "packed/textures/floor.png", &component_asset_ref::path));
    EXPECT_TRUE(std::ranges::contains(moved, "packed/textures/skin.png", &component_asset_ref::path));

    /// and nothing else in the payload was lost to the rewrite
    scene loaded("Asset Scene Again");
    instantiate_scene(loaded, doc, default_codec_services());
    ostd::vector<behavior_fields::field> fields = {};
    const ostd::vector<uint8_t> stream = fields_of(loaded, "Holder", "FieldsBehavior");
    ASSERT_TRUE(behavior_fields::read(stream, fields));
    EXPECT_EQ(find(fields, "icon")->text(), "packed/textures/floor.png");
    EXPECT_EQ(find(fields, "gait")->text(), "Sprint");
  }

  TEST_F(behavior_fields_tests, a_rebuild_of_the_scripts_carries_the_values) {
    scene s("Rebuild Scene");
    make_pair(s);
    probe("SetAll");
    point_holder_at_held(s);
    const integer_t holder_id = script_id_of(s, id_of(s, "Holder"));
    /// after a rebuild nobody says which scene an id is of: the environment is asked
    env->set_behavior_object_resolver([this, &s](natural_t object_id) -> void* {
      return env->get_object_instance(script_id_of(s, object_id));
    });

    env->unload_dotnet_module(env->get_dotnet_module("DotnetTesting"));
    const script_object* holder = env->get_object(holder_id);
    ASSERT_EQ(holder->behavior_handles.size(), 1u);
    /// the instance went with its assembly; what it held is kept
    EXPECT_EQ(holder->behavior_handles[0].instance, nullptr);
    EXPECT_FALSE(holder->behavior_handles[0].fields.empty());
    /// and a save made now still has it
    EXPECT_EQ(fields_of(s, "Holder", "FieldsBehavior"), holder->behavior_handles[0].fields);

    /// back where the fixture expects its copy, which TearDown unloads by its path
    ASSERT_NE(env->load_dotnet_module(perform_tag_replacement("build/script-testing/${build-config}/DotnetTesting.dll")), nullptr);
    env->get_dotnet_host().rediscover_binding_points();
    env->reattach_invalidated_dotnet_behaviors();

    ASSERT_NE(holder->behavior_handles[0].instance, nullptr);
    EXPECT_TRUE(holder->behavior_handles[0].fields.empty());
    EXPECT_EQ(probe("CheckAll"), 0);
    expect_holder_points_at_held(s);
    env->set_behavior_object_resolver(nullptr);
  }

}  // namespace other
