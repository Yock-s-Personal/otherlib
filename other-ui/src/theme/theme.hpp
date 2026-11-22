/**
 * \file theme/theme.hpp
 * a theme file, read. its colours go into the colour tokens windows already draw with; its
 * spacing and fonts are handed back for whoever sets ImGui up
 **/
#ifndef OTHER_UI_THEME_THEME_HPP
#define OTHER_UI_THEME_THEME_HPP

#include <array>
#include <string>
#include <string_view>

#include <glm/glm.hpp>

#include "core/defines.hpp"
#include "data-structures/std_container.hpp"

#include "theme/font.hpp"
#include "theme/theme_tokens.hpp"
#include "theme/ui_theme_settings.hpp"

struct ImGuiStyle;

namespace other {

  /// what a theme holds besides its colours
  struct ui_theme {
    ui_theme_settings metrics;
    /// a role with no file is not given by the theme
    std::array<font_spec, static_cast<size_t>(font_role::NUM_ROLES)> fonts;
  };

  struct theme_load_result {
    /// false when there was no file, or it could not be read as one: nothing was changed
    bool loaded = false;
    uint32_t colors_set = 0;
    /// what in the file was not taken, a line each. everything else in it was
    ostd::vector<std::string> problems;
  };

  /// null when no token has the name
  const color_token* find_color_token(const std::string_view name);
  /// every token back to the colour it was built with
  void reset_color_tokens();

  /// [colors] by token name, as "#RRGGBB", "#RRGGBBAA" or three or four numbers from 0 to
  ///  1; [metrics] into the theme's metrics; [typography] into its font roles. tokens the
  ///  text does not name keep the colour they have
  theme_load_result load_theme_text(const std::string_view text, ui_theme& theme);
  theme_load_result load_theme_file(const filepath& file, ui_theme& theme);

  /// the theme's metrics, and every ImGui colour from the token that stands for it
  void apply_theme(const ui_theme& theme, ImGuiStyle& style);

}  // namespace other

#endif  // OTHER_UI_THEME_THEME_HPP
