/**
 * \file tests/scripting/script_boundary_tests.cpp
 *
 * the crossing between C# and native code: a binding is only made when its two sides agree,
 * strings cross whole in both directions, and a script can listen to engine events
 **/
#include <cstring>
#include <thread>

#include <asio/asio.hpp>
#include <glm/glm.hpp>

#include "event/event_system.hpp"
#include "thread/thread_safety.hpp"

#include "dotnet/binding_signature.hpp"
#include "script/scripting_environment.hpp"

#include "scripting/dotnet_bindings/event_bindings.hpp"
#include "scripting/main_thread_only.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    int32_t probe_right(int32_t value) {
      return value + 1;
    }

    int32_t probe_one_argument(int32_t value) {
      return value;
    }

    int32_t probe_three_floats(float, float, float) {
      return 0;
    }

    const char* probe_echo(const char* text) {
      return bindings::returned_string(text);
    }

    int32_t probe_length(const char* text) {
      return static_cast<int32_t>(std::strlen(text));
    }

  }  // namespace

  class script_boundary_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    void SetUp() override {
      other_test::SetUp();
      env = subsystem<scripting_environment>::get();
      ASSERT_NE(env, nullptr);
      /// the testing assembly is loaded after the engine's functions were bound: its fields
      ///  are found now
      env->get_dotnet_host().rediscover_binding_points();
      bindings::set_script_event_system(&events);
    }

    void TearDown() override {
      bindings::set_script_event_system(nullptr);
      disable_thread_check();
      other_test::TearDown();
    }

    template <typename R, typename... Args>
    R probe(const std::string_view type, const std::string_view method, Args&&... args) {
      return env->call_static_dotnet_method<R>(type, method, std::forward<Args>(args)...);
    }

    scripting_environment* env = nullptr;
    asio::io_context io_context;
    event_system events{ io_context };
  };

  TEST_F(script_boundary_tests, a_signature_is_written_from_the_function_itself) {
    EXPECT_EQ(bindings::signature_of(&probe_right), "(i32)->i32");
    EXPECT_EQ(bindings::signature_of(&probe_three_floats), "(f32,f32,f32)->i32");
    /// a string is a pointer to bytes, in and out
    EXPECT_EQ(bindings::signature_of(&probe_echo), "(ptr<u8>)->ptr<u8>");
    /// nbool32 and natural_t are the integers they are made of; a pointer to a class is a handle
    EXPECT_EQ(bindings::signature_of(&bindings::native_event_subscribe), "(ptr<u8>)->u64");
    EXPECT_EQ(bindings::signature_of(&bindings::set_script_event_system), "(ptr)->void");
    EXPECT_EQ(bindings::signature_of(&bindings::native_event_trigger_with_value), "(ptr<u8>,i32,ptr)->void");
  }

  TEST_F(script_boundary_tests, a_binding_is_made_only_when_its_two_sides_agree) {
    dotnet_host& host = env->get_dotnet_host();
    EXPECT_EQ(probe<int32_t>("BindingProbe", "BoundMask"), 0);

    EXPECT_EQ(bindings::bind_to_script<probe_right>(host, "TestBindingRight"), binding_result::BOUND);
    EXPECT_EQ(probe<int32_t>("BindingProbe", "CallRight", int32_t{ 4 }), 5);

    /// the field takes two arguments, or returns a float, where the function does not
    EXPECT_EQ(bindings::bind_to_script<probe_one_argument>(host, "TestBindingWrongCount"), binding_result::SIGNATURE_DIFFERS);
    EXPECT_EQ(bindings::bind_to_script<probe_one_argument>(host, "TestBindingWrongResult"), binding_result::SIGNATURE_DIFFERS);
    /// the field is a function pointer in the managed convention
    EXPECT_EQ(bindings::bind_to_script<probe_one_argument>(host, "TestBindingManaged"), binding_result::NOT_UNMANAGED_CDECL);
    /// the field takes a struct by value, which no native signature can say
    EXPECT_EQ(bindings::bind_to_script<probe_three_floats>(host, "TestBindingStruct"), binding_result::SIGNATURE_DIFFERS);
    EXPECT_EQ(bindings::bind_to_script<probe_one_argument>(host, "TestBindingNobodyDeclared"), binding_result::NO_FIELD);

    /// only the one that agreed holds a function
    EXPECT_EQ(probe<int32_t>("BindingProbe", "BoundMask"), 1);
  }

  TEST_F(script_boundary_tests, a_string_crosses_whole_in_both_directions) {
    dotnet_host& host = env->get_dotnet_host();
    ASSERT_EQ(bindings::bind_to_script<probe_echo>(host, "TestBindingEcho"), binding_result::BOUND);
    ASSERT_EQ(bindings::bind_to_script<probe_length>(host, "TestBindingLength"), binding_result::BOUND);

    /// plain, not plain, empty, null, and longer than the stack buffer it is first tried in
    for (int32_t which = 0; which < 5; ++which) {
      EXPECT_EQ(probe<int32_t>("BindingProbe", "EchoCase", which), 1) << "case " << which;
    }
    /// text for native code to edit: whole when it fits, cut at a letter when it does not
    for (const int32_t capacity : { 64, 9, 8, 4, 1 }) {
      EXPECT_EQ(probe<int32_t>("BindingProbe", "BufferCase", capacity), 1) << "capacity " << capacity;
    }
  }

  TEST_F(script_boundary_tests, a_script_hears_an_event_with_its_data) {
    ASSERT_EQ(probe<int32_t>("EventProbe", "SubscribeAll"), 7);
    EXPECT_EQ(bindings::script_subscription_count(), 7u);

    events.trigger_event("probe.plain");
    events.trigger_event("probe.plain");
    EXPECT_EQ(probe<int32_t>("EventProbe", "PlainCalls"), 2);

    /// one event, two handlers: one takes the id as it is, one as an int because it fits
    events.trigger_event("probe.u64", uint64_t{ 42 });
    EXPECT_EQ(probe<uint64_t>("EventProbe", "LastU64"), 42u);
    EXPECT_EQ(probe<int32_t>("EventProbe", "LastInt"), 42);
    /// and when it does not fit, the int handler is not called with a wrong number
    events.trigger_event("probe.u64", uint64_t{ 5'000'000'000 });
    EXPECT_EQ(probe<uint64_t>("EventProbe", "LastU64"), 5'000'000'000u);
    EXPECT_EQ(probe<int32_t>("EventProbe", "LastInt"), 42);

    events.trigger_event("probe.text", std::string{ "gr\xC3\xBC\xC3\x9F"
                                                    "e from an event" });
    EXPECT_EQ(probe<int32_t>("EventProbe", "TextIs", int32_t{ 1 }), 1);
    /// the handler on the same event that asked for a float was never called
    EXPECT_EQ(probe<int32_t>("EventProbe", "WrongCalls"), 0);

    events.trigger_event("probe.vec3", glm::vec3{ 1.f, 2.f, 3.f });
    EXPECT_EQ(probe<int32_t>("EventProbe", "Vec3IsOneTwoThree"), 1);
    events.trigger_event("probe.bool", true);
    EXPECT_EQ(probe<int32_t>("EventProbe", "LastBool"), 1);
  }

  TEST_F(script_boundary_tests, a_subscription_that_was_disposed_hears_nothing) {
    ASSERT_EQ(probe<int32_t>("EventProbe", "SubscribeAll"), 7);
    events.trigger_event("probe.u64", uint64_t{ 7 });
    ASSERT_EQ(probe<uint64_t>("EventProbe", "LastU64"), 7u);

    EXPECT_EQ(probe<int32_t>("EventProbe", "StopListeningForU64"), 1);
    EXPECT_EQ(bindings::script_subscription_count(), 6u);
    events.trigger_event("probe.u64", uint64_t{ 8 });
    EXPECT_EQ(probe<uint64_t>("EventProbe", "LastU64"), 7u);
    /// the other handler on the same event still hears it
    EXPECT_EQ(probe<int32_t>("EventProbe", "LastInt"), 8);
  }

  TEST_F(script_boundary_tests, an_event_raised_on_another_thread_reaches_the_script_on_the_main_one) {
    ASSERT_EQ(probe<int32_t>("EventProbe", "SubscribeAll"), 7);
    /// the suite runs with the thread check off; this is about which thread calls the script
    register_main_thread();
    enable_thread_check();

    std::thread other_thread([this]() {
      events.trigger_event("probe.u64", uint64_t{ 99 });
    });
    other_thread.join();
    /// kept, with its data, and not delivered from that thread
    EXPECT_EQ(probe<uint64_t>("EventProbe", "LastU64"), 0u);

    bindings::deliver_queued_script_events();
    EXPECT_EQ(probe<uint64_t>("EventProbe", "LastU64"), 99u);
    /// once
    bindings::deliver_queued_script_events();
    EXPECT_EQ(probe<int32_t>("EventProbe", "LastInt"), 99);
  }

  TEST_F(script_boundary_tests, a_script_raises_an_event_native_code_hears_with_its_kind) {
    value heard_id;
    value heard_text;
    value heard_vec;
    value heard_bool;
    for (const std::string_view name : { "probe.from-script.u64", "probe.from-script.text", "probe.from-script.vec3", "probe.from-script.bool" }) {
      events.register_event(name);
    }
    events.add_listener("probe.from-script.u64", [&heard_id](const value& data) { heard_id = data; });
    events.add_listener("probe.from-script.text", [&heard_text](const value& data) { heard_text = data; });
    events.add_listener("probe.from-script.vec3", [&heard_vec](const value& data) { heard_vec = data; });
    events.add_listener("probe.from-script.bool", [&heard_bool](const value& data) { heard_bool = data; });

    ASSERT_EQ(probe<int32_t>("EventProbe", "RaiseFromScript"), 1);

    /// native listeners assert on the kind they expect, so a script's ulong must be a UINT64
    ASSERT_EQ(heard_id.type(), value_type::UINT64);
    EXPECT_EQ(static_cast<const uint64_t&>(heard_id), 77u);
    ASSERT_EQ(heard_text.type(), value_type::STRING);
    EXPECT_EQ(heard_text.as_string(), "from a script");
    ASSERT_EQ(heard_vec.type(), value_type::VEC3);
    EXPECT_EQ(static_cast<const glm::vec3&>(heard_vec), glm::vec3(4.f, 5.f, 6.f));
    ASSERT_EQ(heard_bool.type(), value_type::OEBOOL);
    EXPECT_TRUE(static_cast<const bool&>(heard_bool));
  }

  TEST_F(script_boundary_tests, unloading_an_assembly_takes_its_subscriptions_and_its_bindings_with_it) {
    dotnet_host& host = env->get_dotnet_host();
    ASSERT_EQ(bindings::bind_to_script<probe_right>(host, "TestBindingRight"), binding_result::BOUND);
    ASSERT_EQ(probe<int32_t>("EventProbe", "SubscribeAll"), 7);
    ASSERT_EQ(bindings::script_subscription_count(), 7u);

    env->unload_dotnet_module(env->get_dotnet_module("DotnetTesting"));
    EXPECT_EQ(bindings::script_subscription_count(), 0u);
    /// nothing is left to call into the assembly that went
    events.trigger_event("probe.plain");
    /// and its fields are forgotten: there is nothing to bind to until it is loaded and found again
    EXPECT_EQ(bindings::bind_to_script<probe_right>(host, "TestBindingRight"), binding_result::NO_FIELD);

    /// back where the fixture expects its copy, which TearDown unloads by its path
    ASSERT_NE(env->load_dotnet_module(perform_tag_replacement("build/script-testing/${build-config}/DotnetTesting.dll")), nullptr);
    host.rediscover_binding_points();
    EXPECT_EQ(bindings::bind_to_script<probe_right>(host, "TestBindingRight"), binding_result::BOUND);
    EXPECT_EQ(probe<int32_t>("BindingProbe", "CallRight", int32_t{ 9 }), 10);
  }

}  // namespace other
