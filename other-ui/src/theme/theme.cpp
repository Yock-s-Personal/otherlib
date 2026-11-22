/**
 * \file theme/theme.cpp
 **/
#include "theme/theme.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include <imgui/imgui.h>
#include <toml++/toml.hpp>

#include "core/profiler.hpp"

namespace other {
  namespace {

    constexpr int64_t kSchemaVersion = 1;
    constexpr float kMinDensity = 0.75f;
    constexpr float kMaxDensity = 1.5f;

    opt<float> number(const toml::node& node) {
      if (const opt<double> value = node.value<double>(); value.has_value()) {
        return static_cast<float>(*value);
      }
      return std::nullopt;
    }

    opt<glm::vec2> pair(const toml::node& node) {
      const toml::array* values = node.as_array();
      if (values == nullptr || values->size() != 2) {
        return std::nullopt;
      }
      const opt<float> x = number((*values)[0]);
      const opt<float> y = number((*values)[1]);
      if (!x.has_value() || !y.has_value()) {
        return std::nullopt;
      }
      return glm::vec2{ *x, *y };
    }

    opt<glm::vec4> hex_color(std::string_view text) {
      if (!text.starts_with('#') || (text.size() != 7 && text.size() != 9)) {
        return std::nullopt;
      }
      text.remove_prefix(1);

      glm::vec4 color{ 0.f, 0.f, 0.f, 1.f };
      for (size_t channel = 0; channel * 2 < text.size(); ++channel) {
        uint32_t byte = 0;
        const char* first = text.data() + channel * 2;
        const auto [end, error] = std::from_chars(first, first + 2, byte, 16);
        if (error != std::errc{} || end != first + 2) {
          return std::nullopt;
        }
        color[static_cast<glm::length_t>(channel)] = static_cast<float>(byte) / 255.f;
      }
      return color;
    }

    opt<glm::vec4> color(const toml::node& node) {
      if (const opt<std::string> text = node.value<std::string>(); text.has_value()) {
        return hex_color(*text);
      }

      const toml::array* values = node.as_array();
      if (values == nullptr || (values->size() != 3 && values->size() != 4)) {
        return std::nullopt;
      }
      glm::vec4 result{ 0.f, 0.f, 0.f, 1.f };
      for (size_t channel = 0; channel < values->size(); ++channel) {
        const opt<float> value = number((*values)[channel]);
        if (!value.has_value() || *value < 0.f || *value > 1.f) {
          return std::nullopt;
        }
        result[static_cast<glm::length_t>(channel)] = *value;
      }
      return result;
    }

    void read_colors(const toml::table& colors, theme_load_result& result) {
      for (const auto& [key, node] : colors) {
        const std::string_view name = key.str();
        const color_token* token = find_color_token(name);
        if (token == nullptr) {
          result.problems.push_back(std::format("[colors] '{}' is not the name of a colour token", name));
          continue;
        }

        const opt<glm::vec4> value = color(node);
        if (!value.has_value()) {
          result.problems.push_back(std::format("[colors] '{}' is not a colour: write \"#RRGGBB\", \"#RRGGBBAA\", or three or four numbers from 0 to 1", name));
          continue;
        }
        *token->ptr = *value;
        ++result.colors_set;
      }
    }

    void read_metrics(const toml::table& metrics, ui_theme_settings& settings, theme_load_result& result) {
      const auto one = [&result](const std::string_view name, const toml::node& node, float& into) {
        if (const opt<float> value = number(node); value.has_value()) {
          into = *value;
        } else {
          result.problems.push_back(std::format("[metrics] '{}' is not a number", name));
        }
      };
      const auto two = [&result](const std::string_view name, const toml::node& node, glm::vec2& into) {
        if (const opt<glm::vec2> value = pair(node); value.has_value()) {
          into = *value;
        } else {
          result.problems.push_back(std::format("[metrics] '{}' is not a pair of numbers", name));
        }
      };

      for (const auto& [key, node] : metrics) {
        const std::string_view name = key.str();
        if (name == "density") {
          one(name, node, settings.density);
          settings.density = std::clamp(settings.density, kMinDensity, kMaxDensity);
        } else if (name == "rounding") {
          one(name, node, settings.rounding);
        } else if (name == "border-size") {
          one(name, node, settings.border_size);
        } else if (name == "item-spacing") {
          two(name, node, settings.item_spacing);
        } else if (name == "frame-padding") {
          two(name, node, settings.frame_padding);
        } else if (name == "cell-padding") {
          two(name, node, settings.cell_padding);
        } else if (name == "indent-spacing") {
          one(name, node, settings.indent_spacing);
        } else if (name == "scrollbar-size") {
          one(name, node, settings.scrollbar_size);
        } else if (name == "grab-min-size") {
          one(name, node, settings.grab_min_size);
        } else {
          result.problems.push_back(std::format("[metrics] has no '{}'", name));
        }
      }
    }

    void read_typography(const toml::table& typography, ui_theme& theme, theme_load_result& result) {
      constexpr std::pair<std::string_view, font_role> kRoles[] = {
        { "default", font_role::DEFAULT },
        { "mono", font_role::MONOSPACE },
        { "heading", font_role::HEADING },
        { "small", font_role::SMALL },
        { "icons", font_role::ICONS },
      };

      for (const auto& [key, node] : typography) {
        const std::string_view name = key.str();
        const auto role = std::ranges::find(kRoles, name, &std::pair<std::string_view, font_role>::first);
        const toml::table* spec = node.as_table();
        if (role == std::end(kRoles) || spec == nullptr) {
          result.problems.push_back(std::format("[typography] '{}' is not a font role given as {{ file = \"...\", size = ... }}", name));
          continue;
        }

        font_spec& font = theme.fonts[static_cast<size_t>(role->second)];
        font.file = (*spec)["file"].value_or(std::string{});
        font.size = (*spec)["size"].value_or(font.size);
        if (font.size <= 0.f) {
          result.problems.push_back(std::format("[typography] '{}' has a size that is not above 0", name));
          font = font_spec{};
        }
      }
    }

    struct style_color {
      ImGuiCol col;
      const glm::vec4* token;
    };

    /// the token that stands for each ImGui colour. the colours ImGui leaves see-through
    ///  (child and table row backgrounds, the dimming behind a modal) are left as they are
    // clang-format off
    const style_color kStyleColors[] = {
      { ImGuiCol_Text,                      &ui::colors::kText },
      { ImGuiCol_TextDisabled,              &ui::colors::kTextDisabled },
      { ImGuiCol_WindowBg,                  &ui::colors::kBG1 },
      { ImGuiCol_PopupBg,                   &ui::colors::kPopup },
      { ImGuiCol_Border,                    &ui::colors::kBorder },
      { ImGuiCol_FrameBg,                   &ui::colors::kField },
      { ImGuiCol_FrameBgHovered,            &ui::colors::kFieldHover },
      { ImGuiCol_FrameBgActive,             &ui::colors::kFieldFocused },
      { ImGuiCol_TitleBg,                   &ui::colors::kTitleBar },
      { ImGuiCol_TitleBgActive,             &ui::colors::kTitleBarActive },
      { ImGuiCol_TitleBgCollapsed,          &ui::colors::kTitleBar },
      { ImGuiCol_MenuBarBg,                 &ui::colors::kMenuBar },
      { ImGuiCol_ScrollbarBg,               &ui::colors::kBG0 },
      { ImGuiCol_ScrollbarGrab,             &ui::colors::kScrollBar },
      { ImGuiCol_ScrollbarGrabHovered,      &ui::colors::kScrollBarHover },
      { ImGuiCol_ScrollbarGrabActive,       &ui::colors::kScrollBarActive },
      { ImGuiCol_CheckMark,                 &ui::colors::kCheckMark },
      { ImGuiCol_SliderGrab,                &ui::colors::kSliderGrab },
      { ImGuiCol_SliderGrabActive,          &ui::colors::kSliderGrabActive },
      { ImGuiCol_Button,                    &ui::colors::kButton },
      { ImGuiCol_ButtonHovered,             &ui::colors::kButtonHover },
      { ImGuiCol_ButtonActive,              &ui::colors::kButtonActive },
      { ImGuiCol_Header,                    &ui::colors::kHeader },
      { ImGuiCol_HeaderHovered,             &ui::colors::kHeaderHover },
      { ImGuiCol_HeaderActive,              &ui::colors::kHeaderActive },
      { ImGuiCol_Separator,                 &ui::colors::kSeparator },
      { ImGuiCol_SeparatorHovered,          &ui::colors::kSeparatorHover },
      { ImGuiCol_SeparatorActive,           &ui::colors::kSeparatorActive },
      { ImGuiCol_ResizeGrip,                &ui::colors::kResizeGrip },
      { ImGuiCol_ResizeGripHovered,         &ui::colors::kResizeGripHover },
      { ImGuiCol_ResizeGripActive,          &ui::colors::kResizeGripActive },
      { ImGuiCol_InputTextCursor,           &ui::colors::kTextBright },
      { ImGuiCol_Tab,                       &ui::colors::kTab },
      { ImGuiCol_TabHovered,                &ui::colors::kTabHover },
      { ImGuiCol_TabSelected,               &ui::colors::kTabActive },
      { ImGuiCol_TabSelectedOverline,       &ui::colors::kAccent },
      { ImGuiCol_TabDimmed,                 &ui::colors::kTabUnfocused },
      { ImGuiCol_TabDimmedSelected,         &ui::colors::kTab },
      { ImGuiCol_TabDimmedSelectedOverline, &ui::colors::kBorder },
      { ImGuiCol_DockingPreview,            &ui::colors::kDockingPreview },
      { ImGuiCol_DockingEmptyBg,            &ui::colors::kDockingEmptyBG },
      { ImGuiCol_PlotLines,                 &ui::colors::kTextMuted },
      { ImGuiCol_PlotLinesHovered,          &ui::colors::kAccentHover },
      { ImGuiCol_PlotHistogram,             &ui::colors::kAccent },
      { ImGuiCol_PlotHistogramHovered,      &ui::colors::kAccentHover },
      { ImGuiCol_TableHeaderBg,             &ui::colors::kHeader },
      { ImGuiCol_TableBorderStrong,         &ui::colors::kBorder },
      { ImGuiCol_TableBorderLight,          &ui::colors::kBorderSubtle },
      { ImGuiCol_TextLink,                  &ui::colors::kInfo },
      { ImGuiCol_TextSelectedBg,            &ui::colors::kSelection },
      { ImGuiCol_TreeLines,                 &ui::colors::kBorder },
      { ImGuiCol_DragDropTarget,            &ui::colors::kAccent },
      { ImGuiCol_NavCursor,                 &ui::colors::kOutlineFocus },
    };
    // clang-format on

    /// plain container: built before the arena is asked for anything
    const std::unordered_map<std::string_view, const color_token*>& tokens_by_name() {
      static const std::unordered_map<std::string_view, const color_token*> names = []() {
        std::unordered_map<std::string_view, const color_token*> built;
        for (const color_token& token : color_token_table()) {
          built.emplace(token.name, &token);
        }
        return built;
      }();
      return names;
    }

  }  // namespace

  const color_token* find_color_token(const std::string_view name) {
    const auto found = tokens_by_name().find(name);
    return found == tokens_by_name().end() ? nullptr : found->second;
  }

  void reset_color_tokens() {
    for (const color_token& token : color_token_table()) {
      *token.ptr = token.default_value;
    }
  }

  theme_load_result load_theme_text(const std::string_view text, ui_theme& theme) {
    PROFILE_SECTION("load_theme_text");
    theme_load_result result;

    toml::table file;
    try {
      file = toml::parse(text);
    } catch (const std::exception& e) {
      result.problems.push_back(std::format("it is not a theme file: {}", e.what()));
      return result;
    }
    result.loaded = true;

    /// the table is built here, so every token's own colour is on record before one is
    ///  written over
    color_token_table();

    for (const auto& [key, node] : file) {
      const std::string_view name = key.str();
      const toml::table* section = node.as_table();
      if (name == "schema-version") {
        if (const int64_t version = node.value_or(int64_t{ 0 }); version != kSchemaVersion) {
          result.problems.push_back(std::format("schema-version is {}, and this build reads {}", version, kSchemaVersion));
        }
      } else if (name == "colors" && section != nullptr) {
        read_colors(*section, result);
      } else if (name == "metrics" && section != nullptr) {
        read_metrics(*section, theme.metrics, result);
      } else if (name == "typography" && section != nullptr) {
        read_typography(*section, theme, result);
      } else {
        result.problems.push_back(std::format("'{}' is not something a theme file holds: it has [colors], [metrics] and [typography]", name));
      }
    }
    return result;
  }

  theme_load_result load_theme_file(const filepath& file, ui_theme& theme) {
    PROFILE_SECTION("load_theme_file");
    std::ifstream in{ file, std::ios::binary };
    if (!in.is_open()) {
      theme_load_result result;
      result.problems.push_back("the file could not be opened");
      return result;
    }

    std::stringstream text;
    text << in.rdbuf();
    return load_theme_text(text.str(), theme);
  }

  void apply_theme(const ui_theme& theme, ImGuiStyle& style) {
    PROFILE_SECTION("apply_theme");
    theme.metrics.apply(&style);
    for (const style_color& row : kStyleColors) {
      style.Colors[row.col] = ui::colors::rgba_to_imvec4(*row.token);
    }
  }

}  // namespace other
