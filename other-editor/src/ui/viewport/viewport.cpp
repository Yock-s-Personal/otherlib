/**
 * \file ui/viewport/viewport.cpp
 **/
#include "ui/viewport/viewport.hpp"

#include <imgui/imgui.h>

#include "core/profiler.hpp"

#include "renderer/render_pipeline.hpp"

#include "driver/systems/asset_system.hpp"
#include "theme/colors.hpp"
#include "ui/asset_picker.hpp"
#include "ui/script_draw_guard.hpp"
#include "ui/ui_node.hpp"

#include "editor_driver.hpp"
#include "services/editor_bindings.hpp"

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
      const bool scene_view = vp.cam == &editor_ctx.editor_camera;
      if (scene_view) {
        editor_ctx.view.shown = false;
        editor_ctx.view.hovered = false;
        editor_ctx.view.clicked = false;
      }
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
        ///  picture is a drag of the window, which moves it or pulls it out of its dock.
        ///  what a tool draws over the picture afterwards is in front of the button
        ImGui::SetCursorScreenPos(position);
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("##picture", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
        vp.hovered = ImGui::IsItemHovered();
        if (scene_view) {
          /// the item a drop is asked of is the button over the picture
          take_dropped_model();
          draw_scene_view(vp, position, size);
        }
      }

      ImGui::End();
    }

    void viewport::draw_scene_view(const other::viewport& vp, const ImVec2& position, const ImVec2& size) {
      PROFILE_SECTION("viewport::draw_scene_view");
      const ImGuiIO& io = ImGui::GetIO();
      editor_services::scene_view_state& view = editor_ctx.view;
      view.shown = true;
      view.position = { position.x, position.y };
      view.size = { size.x, size.y };
      view.hovered = vp.hovered;
      /// the item asked about is the button over the picture
      /// a press that took hold of a handle ends a drag when it is let go, and is no click
      const bool dragged = io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= io.MouseDragThreshold * io.MouseDragThreshold;
      view.clicked = ImGui::IsItemDeactivated() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && vp.hovered && !dragged && !io.KeyAlt && !editor_ctx.handles.press_taken();

      /// under whatever the tools draw
      draw_icons(position, size);

      if (tools_stopped) {
        ImGui::SetCursorScreenPos(ImVec2{ position.x + 8.f, position.y + 8.f });
        ImGui::PushStyleColor(ImGuiCol_Text, colors::rgba_to_imvec4(colors::kFriendlyErrorRed));
        ImGui::TextWrapped("The tools of this view were stopped after an error while they drew: %s", tools_stopped_reason.c_str());
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(ImVec2{ position.x + 8.f, ImGui::GetCursorScreenPos().y });
        if (ImGui::Button("Run them again")) {
          tools_stopped = false;
          tools_stopped_reason.clear();
        }
        return;
      }

      /// the tools ask for their handles while they run, and draw them over the picture
      ///  and not past its edges
      editor_ctx.handles.begin_frame({
        .cam = &editor_ctx.editor_camera,
        .view_size = view.size,
        .pointer = { io.MousePos.x - position.x, io.MousePos.y - position.y },
        .over_view = vp.hovered,
        .down = io.MouseDown[ImGuiMouseButton_Left],
        .pressed = vp.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left),
        .blocked = io.KeyAlt,
        .cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false),
      });
      ImGui::PushClipRect(position, ImVec2{ position.x + size.x, position.y + size.y }, true);

      /// a tool catches what it throws; what is left for here is what ImGui objects to
      const script_draw_errors errors = guard_script_draw([]() { bindings::run_scene_view_function(); });

      ImGui::PopClipRect();
      editor_ctx.handles.end_frame();
      if (errors.count > 0) {
        tools_stopped = true;
        tools_stopped_reason = errors.describe();
        CORE_LOG_ERROR("The Scene View's tools are stopped until they are asked to run again. {}", tools_stopped_reason);
      }
    }

    void viewport::draw_icons(const ImVec2& position, const ImVec2& size) {
      PROFILE_SECTION("viewport::draw_icons");
      /// glyphs of the Nerd Font the UI is drawn with: a light bulb, a sun, a film camera
      constexpr const char* kGlyphs[] = { "\xEF\x83\xAB", "\xEF\x86\x85", "\xEF\x80\xBD" };
      constexpr float kBack = 11.f;

      editor_ctx.collect_view_icons(icons);
      if (icons.empty()) {
        return;
      }

      ImDrawList* list = ImGui::GetWindowDrawList();
      list->PushClipRect(position, ImVec2{ position.x + size.x, position.y + size.y }, true);
      const glm::vec4& chosen = editor_ctx.settings.selection_outline_color;
      for (const editor_services::view_icon& icon : icons) {
        const ImVec2 middle{ position.x + icon.place.x, position.y + icon.place.y };
        const char* glyph = kGlyphs[icon.what];
        const ImVec2 glyph_size = ImGui::CalcTextSize(glyph);
        /// a dark disc behind it, so that it is read against anything
        list->AddCircleFilled(middle, kBack, IM_COL32(20, 20, 22, 190));
        if (icon.selected) {
          list->AddCircle(middle, kBack, ImGui::ColorConvertFloat4ToU32(ImVec4{ chosen.r, chosen.g, chosen.b, chosen.a }), 0, 2.f);
        }
        list->AddText(ImVec2{ middle.x - glyph_size.x * 0.5f, middle.y - glyph_size.y * 0.5f }, icon.selected ? ImGui::ColorConvertFloat4ToU32(ImVec4{ chosen.r, chosen.g, chosen.b, chosen.a }) : IM_COL32(235, 235, 235, 255), glyph);
      }
      list->PopClipRect();
    }

    void viewport::take_dropped_model() {
      if (!ImGui::BeginDragDropTarget()) {
        return;
      }

      if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(inspector::kAssetDragDropPayloadType); dropped != nullptr && dropped->Data != nullptr) {
        const auto* data = static_cast<const inspector::asset_drag_drop_payload*>(dropped->Data);
        std::string path = data->path;
        /// an asset the engine already holds is dragged by its id: its path is asked for
        if (path.empty() && data->handler_asset_id != 0) {
          const auto& assets = get_driver().get_kernel().get_core_system<asset_system>().get_asset_manager();
          if (const asset* held = assets->get_loaded_asset(data->handler_asset_id); held != nullptr) {
            path = held->load_path.generic_string();
          }
        }

        if (data->asset_type != asset::MODEL_SOURCE) {
          CORE_LOG_INFO("Only a model can be dropped into the Scene View; '{}' is not one.", path);
        } else if (!path.empty()) {
          const ImVec2 at = ImGui::GetMousePos();
          editor_ctx.place_model(path, { at.x, at.y });
        }
      }
      ImGui::EndDragDropTarget();
    }

  }  // namespace ui
}  // namespace other
