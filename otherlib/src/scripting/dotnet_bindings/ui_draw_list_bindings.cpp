/**
 * \file scripting/dotnet_bindings/ui_draw_list_bindings.cpp
 **/
#include "scripting/dotnet_bindings/ui_draw_list_bindings.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "file/path_helpers.hpp"

#include "gpu_resource/texture.hpp"
#include "renderer/renderer.hpp"
#include "renderer/renderer_backend.hpp"

#include "driver/driver.hpp"
#include "driver/systems/asset_system.hpp"
#include "scripting/dotnet_bindings/driver_bindings.hpp"

#include "asset/asset.hpp"

namespace other {
  namespace bindings {

    namespace {

      /// clip rectangles scripts have pushed and not popped yet
      natural_t open_clip_rects = 0;

      void no_size(int32_t* out_width, int32_t* out_height) {
        *out_width = 0;
        *out_height = 0;
      }

      renderer* driver_renderer() {
        driver* d = detail::get_dotnet_native_driver_unchecked();
        return d != nullptr && d->rendering_enabled() ? &d->get_renderer() : nullptr;
      }

      natural_t texture_of(renderer& r, const resource_handle& handle, int32_t* out_width, int32_t* out_height) {
        if (!r.resource_exists(handle)) {
          return 0;
        }

        texture& found = r.get_resource<texture>(handle);
        *out_width = found.get_size().x;
        *out_height = found.get_size().y;
        return static_cast<natural_t>(found.get_imgui_texture_id());
      }

    }  // namespace

    void native_ui_draw_line(float x1, float y1, float x2, float y2, uint32_t color, float thickness) {
      ImGui::GetWindowDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, thickness);
    }

    void native_ui_draw_rect(float min_x, float min_y, float max_x, float max_y, uint32_t color, float rounding, float thickness) {
      ImGui::GetWindowDrawList()->AddRect(ImVec2(min_x, min_y), ImVec2(max_x, max_y), color, rounding, ImDrawFlags_None, thickness);
    }

    void native_ui_draw_rect_filled(float min_x, float min_y, float max_x, float max_y, uint32_t color, float rounding) {
      ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min_x, min_y), ImVec2(max_x, max_y), color, rounding);
    }

    void native_ui_draw_circle(float x, float y, float radius, uint32_t color, int32_t segments, float thickness) {
      ImGui::GetWindowDrawList()->AddCircle(ImVec2(x, y), radius, color, segments, thickness);
    }

    void native_ui_draw_circle_filled(float x, float y, float radius, uint32_t color, int32_t segments) {
      ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(x, y), radius, color, segments);
    }

    void native_ui_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t color, float thickness) {
      ImGui::GetWindowDrawList()->AddTriangle(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), color, thickness);
    }

    void native_ui_draw_triangle_filled(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t color) {
      ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), color);
    }

    void native_ui_draw_bezier_cubic(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, uint32_t color, float thickness, int32_t segments) {
      ImGui::GetWindowDrawList()->AddBezierCubic(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ImVec2(x4, y4), color, thickness, segments);
    }

    void native_ui_draw_text(float x, float y, uint32_t color, const char* text) {
      ImGui::GetWindowDrawList()->AddText(ImVec2(x, y), color, text);
    }

    void native_ui_draw_image(natural_t texture, float min_x, float min_y, float max_x, float max_y, float u0, float v0, float u1, float v1, uint32_t tint) {
      if (texture == 0) {
        return;
      }
      ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(texture), ImVec2(min_x, min_y), ImVec2(max_x, max_y), ImVec2(u0, v0), ImVec2(u1, v1), tint);
    }

    void native_ui_push_clip_rect(float min_x, float min_y, float max_x, float max_y, nbool32 intersect_with_current) {
      ImGui::PushClipRect(ImVec2(min_x, min_y), ImVec2(max_x, max_y), intersect_with_current != 0);
      ++open_clip_rects;
    }

    void native_ui_pop_clip_rect() {
      /// popping one the window itself pushed would take ImGui down with it
      if (open_clip_rects == 0) {
        IM_ASSERT_USER_ERROR(false, "Calling PopClipRect() too many times!");
        return;
      }
      ImGui::PopClipRect();
      --open_clip_rects;
    }

    natural_t close_script_clip_rects() {
      const natural_t left_open = open_clip_rects;
      for (; open_clip_rects > 0; --open_clip_rects) {
        ImGui::PopClipRect();
      }
      return left_open;
    }

    natural_t native_ui_get_pipeline_texture(const char* pipeline, const char* name, int32_t* out_width, int32_t* out_height) {
      no_size(out_width, out_height);
      renderer* r = driver_renderer();
      if (r == nullptr) {
        return 0;
      }

      render_pipeline* found = r->get_pipeline(pipeline);
      if (found == nullptr) {
        return 0;
      }
      const opt<resource_handle> handle = found->find_texture_by_name(name);
      return handle.has_value() ? texture_of(*r, *handle, out_width, out_height) : 0;
    }

    natural_t native_ui_get_asset_texture(natural_t asset_id, int32_t* out_width, int32_t* out_height) {
      no_size(out_width, out_height);
      renderer* r = driver_renderer();
      if (r == nullptr || asset_id == 0) {
        return 0;
      }

      const asset* found = detail::get_dotnet_native_driver_unchecked()->get_asset(asset_id);
      if (found == nullptr || found->asset_type != asset::TEXTURE) {
        return 0;
      }
      /// a texture that has not finished loading has no handle yet
      const resource_handle handle = subsystem<renderer_backend>::get()->get_texture(found->path_hash);
      return handle.id != 0 ? texture_of(*r, handle, out_width, out_height) : 0;
    }

    natural_t native_ui_get_asset_texture_by_path(const char* path, int32_t* out_width, int32_t* out_height) {
      no_size(out_width, out_height);
      driver* d = detail::get_dotnet_native_driver_unchecked();
      if (d == nullptr || !d->get_kernel().has_core_system<asset_system>()) {
        return 0;
      }

      const asset_path resolved = resolve_asset_path(path);
      if (!resolved.found()) {
        return 0;
      }
      const natural_t asset_id = d->get_kernel().get_core_system<asset_system>().get_asset_id_from_path(resolved.resolved);
      return native_ui_get_asset_texture(asset_id, out_width, out_height);
    }

    void native_ui_image(natural_t texture, float width, float height, float u0, float v0, float u1, float v1) {
      if (texture == 0) {
        ImGui::Dummy(ImVec2(width, height));
        return;
      }
      ImGui::Image(static_cast<ImTextureID>(texture), ImVec2(width, height), ImVec2(u0, v0), ImVec2(u1, v1));
    }

  }  // namespace bindings
}  // namespace other
