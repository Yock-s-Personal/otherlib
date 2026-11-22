/**
 * \file tests/ui/theme_tests.cpp
 *
 * a theme file is read into the colour tokens windows draw with, the metrics and the font
 * roles, and what it gets wrong is said without the rest being lost
 **/
#include <filesystem>
#include <fstream>

#include <imgui/imgui.h>

#include "script/scripting_environment.hpp"

#include "theme/theme.hpp"

#include "headless_imgui.hpp"
#include "other_test.hpp"

namespace other {
  namespace {

    bool mentions(const theme_load_result& result, const std::string_view text) {
      return std::ranges::any_of(result.problems, [text](const std::string& problem) { return problem.find(text) != std::string::npos; });
    }

  }  // namespace

  class theme_tests : public other_test {
   protected:
    bool script_and_physics() const override { return true; }

    void TearDown() override {
      /// the tokens are the engine's own colours, and every other test's
      reset_color_tokens();
      other_test::TearDown();
    }

    ui_theme theme;
  };

  TEST_F(theme_tests, a_colour_is_written_into_the_token_of_its_name) {
    const glm::vec4 built_with = ui::colors::kAccent;
    const theme_load_result result = load_theme_text(
      "[colors]\n"
      "\"core.accent\" = \"#3BA7F0\"\n"
      "\"core.bg1\" = \"#0B0E1480\"\n"
      "\"inspector.field_bg\" = [0.25, 0.5, 0.75]\n"
      "\"core.text\" = [0.1, 0.2, 0.3, 0.4]\n",
      theme);

    EXPECT_TRUE(result.loaded);
    EXPECT_EQ(result.colors_set, 4u);
    EXPECT_TRUE(result.problems.empty());

    /// the variable windows read is the one that changed
    EXPECT_FLOAT_EQ(ui::colors::kAccent.r, 0x3B / 255.f);
    EXPECT_FLOAT_EQ(ui::colors::kAccent.g, 0xA7 / 255.f);
    EXPECT_FLOAT_EQ(ui::colors::kAccent.b, 0xF0 / 255.f);
    EXPECT_FLOAT_EQ(ui::colors::kAccent.a, 1.f);
    EXPECT_FLOAT_EQ(ui::colors::kBG1.a, 0x80 / 255.f);
    EXPECT_EQ(ui::colors::inspector::kFieldBG, (glm::vec4{ 0.25f, 0.5f, 0.75f, 1.f }));
    EXPECT_EQ(ui::colors::kText, (glm::vec4{ 0.1f, 0.2f, 0.3f, 0.4f }));

    /// and a token is found by its name, with the colour it was built with on record
    const color_token* accent = find_color_token("core.accent");
    ASSERT_NE(accent, nullptr);
    EXPECT_EQ(accent->ptr, &ui::colors::kAccent);
    EXPECT_EQ(accent->default_value, built_with);
    EXPECT_EQ(find_color_token("core.no_such_token"), nullptr);

    reset_color_tokens();
    EXPECT_EQ(ui::colors::kAccent, built_with);
  }

  TEST_F(theme_tests, what_is_wrong_in_a_file_is_said_and_the_rest_is_taken) {
    const glm::vec4 text_before = ui::colors::kText;
    const theme_load_result result = load_theme_text(
      "schema-version = 7\n"
      "[colors]\n"
      "\"core.accent\" = \"#3BA7F0\"\n"
      "\"core.text\" = \"blue\"\n"
      "\"core.border\" = \"#12345\"\n"
      "\"core.bg0\" = [0.5, 2.0, 0.5]\n"
      "\"core.no_such_token\" = \"#FFFFFF\"\n"
      "[metrics]\n"
      "rounding = \"round\"\n"
      "roundness = 3.0\n"
      "scrollbar-size = 20.0\n"
      "[typography]\n"
      "title = { file = \"a.ttf\", size = 12.0 }\n"
      "small = { file = \"a.ttf\", size = 0.0 }\n"
      "[motion]\n"
      "hover-fade-ms = 80.0\n",
      theme);

    EXPECT_TRUE(result.loaded);
    EXPECT_EQ(result.colors_set, 1u);
    EXPECT_EQ(ui::colors::kText, text_before);
    EXPECT_FLOAT_EQ(theme.metrics.scrollbar_size, 20.f);
    EXPECT_FLOAT_EQ(theme.metrics.rounding, ui_theme_settings{}.rounding);
    EXPECT_TRUE(theme.fonts[static_cast<size_t>(font_role::SMALL)].file.empty());

    EXPECT_EQ(result.problems.size(), 10u);
    EXPECT_TRUE(mentions(result, "schema-version is 7"));
    EXPECT_TRUE(mentions(result, "'core.text' is not a colour"));
    EXPECT_TRUE(mentions(result, "'core.border' is not a colour"));
    EXPECT_TRUE(mentions(result, "'core.bg0' is not a colour"));
    EXPECT_TRUE(mentions(result, "'core.no_such_token' is not the name of a colour token"));
    EXPECT_TRUE(mentions(result, "'rounding' is not a number"));
    EXPECT_TRUE(mentions(result, "has no 'roundness'"));
    EXPECT_TRUE(mentions(result, "'title' is not a font role"));
    EXPECT_TRUE(mentions(result, "'small' has a size"));
    EXPECT_TRUE(mentions(result, "'motion' is not something a theme file holds"));
  }

  TEST_F(theme_tests, metrics_and_font_roles_are_read) {
    const theme_load_result result = load_theme_text(
      "schema-version = 1\n"
      "[metrics]\n"
      "density = 4.0\n"
      "rounding = 6\n"
      "border-size = 0.0\n"
      "item-spacing = [10.0, 5.0]\n"
      "frame-padding = [7.0, 4.0]\n"
      "cell-padding = [3.0, 1.0]\n"
      "indent-spacing = 22.0\n"
      "grab-min-size = 14.0\n"
      "[typography]\n"
      "default = { file = \"fonts/a.ttf\", size = 17.0 }\n"
      "mono = { file = \"fonts/b.ttf\" }\n"
      "icons = { }\n",
      theme);

    EXPECT_TRUE(result.loaded);
    EXPECT_TRUE(result.problems.empty());
    /// a density out of range is brought back into it
    EXPECT_FLOAT_EQ(theme.metrics.density, 1.5f);
    EXPECT_FLOAT_EQ(theme.metrics.rounding, 6.f);
    EXPECT_FLOAT_EQ(theme.metrics.border_size, 0.f);
    EXPECT_EQ(theme.metrics.item_spacing, (glm::vec2{ 10.f, 5.f }));
    EXPECT_EQ(theme.metrics.frame_padding, (glm::vec2{ 7.f, 4.f }));
    EXPECT_EQ(theme.metrics.cell_padding, (glm::vec2{ 3.f, 1.f }));
    EXPECT_FLOAT_EQ(theme.metrics.indent_spacing, 22.f);
    EXPECT_FLOAT_EQ(theme.metrics.grab_min_size, 14.f);

    EXPECT_EQ(theme.fonts[static_cast<size_t>(font_role::DEFAULT)].file, "fonts/a.ttf");
    EXPECT_FLOAT_EQ(theme.fonts[static_cast<size_t>(font_role::DEFAULT)].size, 17.f);
    EXPECT_EQ(theme.fonts[static_cast<size_t>(font_role::MONOSPACE)].file, "fonts/b.ttf");
    EXPECT_FLOAT_EQ(theme.fonts[static_cast<size_t>(font_role::MONOSPACE)].size, 16.f);
    /// a role given as an empty table, or not at all, is not given
    EXPECT_TRUE(theme.fonts[static_cast<size_t>(font_role::ICONS)].file.empty());
    EXPECT_TRUE(theme.fonts[static_cast<size_t>(font_role::HEADING)].file.empty());
  }

  TEST_F(theme_tests, a_file_that_is_missing_or_is_not_one_changes_nothing) {
    const glm::vec4 accent_before = ui::colors::kAccent;

    const theme_load_result missing = load_theme_file("no/such/folder/theme.toml", theme);
    EXPECT_FALSE(missing.loaded);
    EXPECT_EQ(missing.problems.size(), 1u);

    const theme_load_result broken = load_theme_text("[colors\n\"core.accent\" = \"#3BA7F0\"\n", theme);
    EXPECT_FALSE(broken.loaded);
    EXPECT_EQ(broken.colors_set, 0u);
    EXPECT_TRUE(mentions(broken, "it is not a theme file"));

    EXPECT_EQ(ui::colors::kAccent, accent_before);
    EXPECT_FLOAT_EQ(theme.metrics.rounding, ui_theme_settings{}.rounding);
  }

  TEST_F(theme_tests, the_editors_own_theme_file_reads_without_a_problem) {
    const theme_load_result result = load_theme_file("resources/editor-assets/theme.toml", theme);
    ASSERT_TRUE(result.loaded);
    EXPECT_TRUE(result.problems.empty()) << result.problems.front();
    EXPECT_FALSE(theme.fonts[static_cast<size_t>(font_role::DEFAULT)].file.empty());
    EXPECT_FALSE(theme.fonts[static_cast<size_t>(font_role::HEADING)].file.empty());
  }

  TEST_F(theme_tests, a_script_reads_a_tokens_colour_by_its_name) {
    auto* env = subsystem<scripting_environment>::get();
    ASSERT_NE(env, nullptr);
    const auto probe = [env](const std::string_view method, int32_t which) -> int32_t {
      return env->call_static_dotnet_method<int32_t>("ThemeProbe", method, which);
    };

    ASSERT_TRUE(load_theme_text("[colors]\n\"core.accent\" = \"#3BA7F0\"\n\"core.bg1\" = \"#102030\"\n", theme).loaded);
    EXPECT_EQ(probe("Color", 0), 0x3BA7F0);
    EXPECT_EQ(probe("Color", 1), 0x102030);
    EXPECT_EQ(probe("Color", 2), -1);
    /// a name that is no token is drawn magenta
    EXPECT_EQ(env->call_static_dotnet_method<int32_t>("ThemeProbe", "ColorOfNoToken"), 0xFF00FF);

    /// a font role is the same number on both sides
    const font_role roles[] = { font_role::DEFAULT, font_role::MONOSPACE, font_role::HEADING, font_role::SMALL, font_role::ICONS };
    for (int32_t which = 0; which < static_cast<int32_t>(font_role::NUM_ROLES); ++which) {
      EXPECT_EQ(probe("FontRoleValue", which), static_cast<int32_t>(roles[which])) << which;
    }
  }

  TEST_F(theme_tests, imgui_is_given_the_tokens_colours_and_the_metrics) {
    headless_imgui imgui;
    ImGuiStyle& style = ImGui::GetStyle();

    ASSERT_TRUE(load_theme_text(
                  "[colors]\n"
                  "\"core.bg1\" = \"#102030\"\n"
                  "\"core.accent\" = \"#3BA7F0\"\n"
                  "[metrics]\n"
                  "density = 1.5\n"
                  "frame-padding = [6.0, 4.0]\n"
                  "rounding = 4.0\n",
                  theme)
                  .loaded);
    apply_theme(theme, style);

    /// a colour the file set, through the token that stands for it
    const ImVec4 window = style.Colors[ImGuiCol_WindowBg];
    EXPECT_FLOAT_EQ(window.x, 0x10 / 255.f);
    EXPECT_FLOAT_EQ(window.y, 0x20 / 255.f);
    EXPECT_FLOAT_EQ(window.z, 0x30 / 255.f);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_TabSelectedOverline].z, 0xF0 / 255.f);
    /// and one it did not, which is the engine's own
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_Text].x, ui::colors::kText.r);
    EXPECT_FLOAT_EQ(style.Colors[ImGuiCol_Button].x, ui::colors::kButton.r);

    EXPECT_FLOAT_EQ(style.FramePadding.x, 9.f);
    EXPECT_FLOAT_EQ(style.FramePadding.y, 6.f);
    EXPECT_FLOAT_EQ(style.FrameRounding, 6.f);
  }

}  // namespace other
