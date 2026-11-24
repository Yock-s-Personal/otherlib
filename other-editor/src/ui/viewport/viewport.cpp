/**
 * \file ui/viewport/viewport.cpp
 **/
#include "ui/viewport/viewport.hpp"

#include <imgui/imgui.h>

#include "core/profiler.hpp"

#include "renderer/render_pipeline.hpp"

#include "theme/colors.hpp"
#include "ui/ui_node.hpp"

#include "editor_driver.hpp"

namespace other {
  namespace ui {

    viewport::viewport(editor_context& ctx, event_system& events, renderer& renderer_instance, driver* driver_ptr)
        : ui_window(&events, "Viewport", true, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar), renderer_instance(renderer_instance), driver_ptr(driver_ptr), editor_ctx(ctx) {
    }

    void viewport::custom_render() {
      PROFILE_SECTION("viewport::custom_render");
      auto viewports = get_driver().get_kernel().get_core_system<rendering_system>().get_viewports();
      for (auto& vp : viewports) {
        draw_viewport(vp);
      }
    }

    void viewport::draw_viewport(other::viewport& vp) {
      PROFILE_SECTION("viewport::draw_viewport");
      auto& kernel = get_driver().get_kernel();
      auto& rendering_sys = kernel.get_core_system<rendering_system>();

      const bool begin = ImGui::Begin(vp.label.c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
      /// folded, or a tab behind another: the scene is not drawn for it until it shows again
      vp.visible = begin;
      vp.hovered = false;
      if (!begin) {
        ImGui::End();
        return;
      }

      const ImVec2 size = ImGui::GetContentRegionAvail();
      const ImVec2 position = ImGui::GetCursorScreenPos();
      vp.size = glm::ivec2(size.x, size.y);
      vp.screen_position = { position.x, position.y };

      ImTextureID tex_id = rendering_sys.get_texture_id(vp.texture);
      if (tex_id == 0) {
        scoped_color error_color{ ImGuiCol_Text, colors::rgba_to_imvec4(colors::kFriendlyErrorRed) };
        ImGui::Text("Requested viewport texture %s does not exist.", vp.name.c_str());
        tex_id = renderer_instance.get_texture_id("default-instancing", "smaa_texture");
      }

      if (tex_id == 0) {
        scoped_color error_color{ ImGuiCol_Text, colors::rgba_to_imvec4(colors::kFriendlyErrorRed) };
        ImGui::Text("No fallback texture available.");
      } else if (vp.cam == nullptr) {
        scoped_color error_color{ ImGuiCol_Text, colors::rgba_to_imvec4(colors::kFriendlyErrorRed) };
        ImGui::Text("Viewport '%s' has no camera assigned.", vp.name.c_str());
      } else if (size.x >= 1.f && size.y >= 1.f) {
        ImGui::Image(tex_id, size, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

        /// a button over the picture takes the presses on it: without one a drag on the
        ///  picture is a drag of the window, which moves it or pulls it out of its dock
        ImGui::SetCursorScreenPos(position);
        ImGui::InvisibleButton("##picture", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
        vp.hovered = ImGui::IsItemHovered();
      }

      ImGui::End();
    }

  }  // namespace ui
}  // namespace other
