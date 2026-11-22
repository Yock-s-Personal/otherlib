/**
 * \file theme/font.hpp
 **/
#ifndef OTHER_UI_THEME_FONT_HPP
#define OTHER_UI_THEME_FONT_HPP

#include <array>
#include <span>
#include <string>

#include "core/defines.hpp"
#include "data-structures/std_container.hpp"

struct ImFont;

namespace other {

  /// what a font is for. FontRole in other-csharp/src/renderer/UI/theme.cs is the same list
  ///  in the same order
  enum class font_role {
    DEFAULT,
    MONOSPACE,
    HEADING,
    SMALL,
    ICONS,
    NUM_ROLES,
  };

  struct font_spec {
    std::string file;
    float size = 16.f;
  };

  /// the fonts a theme names, by what they are for
  class font_set {
   public:
    /// adds each role's font to ImGui's fonts. a file is looked for as an asset path, from
    ///  theme_file's folder first. the default role replaces ImGui's default font; a role
    ///  with no file, or one whose file cannot be loaded, stays the default font. what could
    ///  not be loaded is added to problems. to be called before the first frame is drawn
    void load(std::span<const font_spec, static_cast<size_t>(font_role::NUM_ROLES)> specs, const filepath& theme_file, ostd::vector<std::string>& problems);
    /// every role back to the default font. the fonts themselves are ImGui's
    void clear();

    /// null for a role that was not loaded: ImGui takes that as its default font
    ImFont* get(font_role role) const;
    /// how many roles have a font of their own
    size_t loaded_count() const;

   private:
    std::array<ImFont*, static_cast<size_t>(font_role::NUM_ROLES)> fonts{};
  };

  /// the fonts of the theme in use. every role is the default font until a theme is loaded
  font_set& ui_fonts();

}  // namespace other

#endif  // OTHER_UI_THEME_FONT_HPP
