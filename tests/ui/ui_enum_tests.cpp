/**
 * \file tests/ui/ui_enum_tests.cpp
 *
 * the C# enums that mirror ImGui's have ImGui's values: an ImGui update cannot move one
 * without this failing, or failing to compile
 **/
#include "input/input_types.hpp"

#include "script/scripting_environment.hpp"

#include "other_test.hpp"
#include "ui_enum_values.hpp"

namespace other {
  namespace {

    /// FNV-1a over the characters, as the probe sums the names it holds
    int32_t name_hash(const std::string_view text) {
      uint32_t hash = 2166136261u;
      for (const char c : text) {
        hash = (hash ^ static_cast<uint8_t>(c)) * 16777619u;
      }
      return static_cast<int32_t>(hash);
    }

  }  // namespace

  class ui_enum_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    template <typename... Args>
    int32_t probe(const std::string_view method, Args... args) {
      auto* env = subsystem<scripting_environment>::get();
      return env->call_static_dotnet_method<int32_t>("UiEnumProbe", method, args...);
    }
  };

  TEST_F(ui_enum_tests, every_member_has_the_value_imgui_gives_it) {
    int32_t which = 0;
    for (const ui_enum& mirrored : kUiEnums) {
      ASSERT_EQ(probe("EnumNameHash", which), name_hash(mirrored.name)) << "enum " << which << " is not " << mirrored.name;
      ASSERT_EQ(probe("Count", which), static_cast<int32_t>(mirrored.members.size())) << mirrored.name;

      int32_t index = 0;
      for (const ui_enum_member& member : mirrored.members) {
        ASSERT_EQ(probe("NameHash", which, index), name_hash(member.name)) << mirrored.name << " has no " << member.name << " where the table has it";
        EXPECT_EQ(probe("Value", which, index), member.value) << mirrored.name << "." << member.name;
        ++index;
      }
      ++which;
    }

    /// and C# mirrors no enum the table does not have
    EXPECT_EQ(probe("Count", which), -1);
  }

  TEST_F(ui_enum_tests, the_values_a_window_is_most_likely_to_pass_are_what_imgui_reads) {
    /// spelled out, so a mistake in what writes the table would have to be made twice
    const auto value_of = [](const ui_enum& mirrored, const std::string_view name) -> int64_t {
      for (const ui_enum_member& member : mirrored.members) {
        if (member.name == name) {
          return member.value;
        }
      }
      return -1;
    };

    EXPECT_EQ(value_of(kUiEnums[0], "MenuBar"), 1 << 10);
    EXPECT_EQ(value_of(kUiEnums[1], "Borders"), 1);
    EXPECT_EQ(value_of(kUiEnums[2], "DefaultOpen"), 1 << 5);
    EXPECT_EQ(value_of(kUiEnums[5], "Text"), 0);
    EXPECT_EQ(value_of(kUiEnums[7], "FirstUseEver"), 4);
    EXPECT_EQ(value_of(kUiEnums[8], "Tab"), 512);
    EXPECT_EQ(value_of(kUiEnums[9], "Ctrl"), 1 << 12);
  }

  TEST_F(ui_enum_tests, a_mouse_button_is_the_same_number_for_imgui_as_for_the_engine) {
    /// UI calls take the engine's own Other.MouseButton and hand its number to ImGui
    EXPECT_EQ(static_cast<int32_t>(ImGuiMouseButton_Left), static_cast<int32_t>(mouse_button::LEFT));
    EXPECT_EQ(static_cast<int32_t>(ImGuiMouseButton_Right), static_cast<int32_t>(mouse_button::RIGHT));
    EXPECT_EQ(static_cast<int32_t>(ImGuiMouseButton_Middle), static_cast<int32_t>(mouse_button::MIDDLE));
    EXPECT_EQ(static_cast<int32_t>(ImGuiMouseButton_COUNT), static_cast<int32_t>(mouse_button::NUM_MOUSE_BUTTONS));
  }

}  // namespace other
