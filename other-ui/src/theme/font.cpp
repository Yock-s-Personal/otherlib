/**
 * \file theme/font.cpp
 **/
#include "theme/font.hpp"

#include <algorithm>
#include <format>
#include <string_view>

#include <imgui/imgui.h>

#include "core/profiler.hpp"
#include "file/path_helpers.hpp"

#include "renderer/renderer_backend.hpp"

namespace other {
  namespace {

    constexpr std::string_view kRoleNames[] = { "default", "mono", "heading", "small", "icons" };
    static_assert(std::size(kRoleNames) == static_cast<size_t>(font_role::NUM_ROLES), "a name for every font role");

    /// the font ImGui already has from that file at that size, so one is not loaded twice
    ImFont* loaded_already(const filepath& file, float size) {
      const std::string name = file.filename().string();
      for (ImFont* font : ImGui::GetIO().Fonts->Fonts) {
        if (font == nullptr || font->FontSize != size || font->Sources == nullptr) {
          continue;
        }
        /// ImGui names a font "<file name>, <size>px"
        if (std::string_view{ font->Sources->Name }.starts_with(name)) {
          return font;
        }
      }
      return nullptr;
    }

  }  // namespace

  void font_set::load(std::span<const font_spec, static_cast<size_t>(font_role::NUM_ROLES)> specs, const filepath& theme_file, ostd::vector<std::string>& problems) {
    PROFILE_SECTION("font_set::load");
    clear();

    ImGuiIO& io = ImGui::GetIO();
    for (size_t role = 0; role < specs.size(); ++role) {
      const font_spec& spec = specs[role];
      if (spec.file.empty()) {
        continue;
      }

      const asset_path found = find_asset_path(spec.file, theme_file);
      if (!found.found()) {
        problems.push_back(std::format("[typography] {}: font '{}' {}", kRoleNames[role], spec.file, found.problem));
        continue;
      }

      ImFont* font = loaded_already(found.resolved, spec.size);
      if (font == nullptr) {
        font = io.Fonts->AddFontFromFileTTF(found.resolved.string().c_str(), spec.size, nullptr, kUnicodeExtraRanges);
      }
      if (font == nullptr) {
        problems.push_back(std::format("[typography] {}: font '{}' could not be loaded", kRoleNames[role], spec.file));
        continue;
      }

      fonts[role] = font;
      if (static_cast<font_role>(role) == font_role::DEFAULT) {
        io.FontDefault = font;
      }
    }
  }

  void font_set::clear() {
    fonts.fill(nullptr);
  }

  ImFont* font_set::get(font_role role) const {
    OTHER_ASSERT(static_cast<size_t>(role) < static_cast<size_t>(font_role::NUM_ROLES), "Invalid font role.");
    return fonts[static_cast<size_t>(role)];
  }

  size_t font_set::loaded_count() const {
    return static_cast<size_t>(std::ranges::count_if(fonts, [](const ImFont* font) { return font != nullptr; }));
  }

  font_set& ui_fonts() {
    static font_set fonts;
    return fonts;
  }

}  // namespace other
