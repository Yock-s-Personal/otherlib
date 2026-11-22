/**
 * \file scripting/dotnet_bindings/ui_draw_list_bindings.hpp
 * drawing in a window beyond its widgets: shapes on the window's draw list, and the
 * engine's textures as images. a colour is ImGui's packed one (IM_COL32), positions are in
 * screen space
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_UI_DRAW_LIST_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_UI_DRAW_LIST_BINDINGS_HPP

#include "core/defines.hpp"

#include "dotnet/types.hpp"

namespace other {
  namespace bindings {

    void native_ui_draw_line(float x1, float y1, float x2, float y2, uint32_t color, float thickness);
    void native_ui_draw_rect(float min_x, float min_y, float max_x, float max_y, uint32_t color, float rounding, float thickness);
    void native_ui_draw_rect_filled(float min_x, float min_y, float max_x, float max_y, uint32_t color, float rounding);
    /// 0 segments lets ImGui pick how many for the radius
    void native_ui_draw_circle(float x, float y, float radius, uint32_t color, int32_t segments, float thickness);
    void native_ui_draw_circle_filled(float x, float y, float radius, uint32_t color, int32_t segments);
    void native_ui_draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t color, float thickness);
    void native_ui_draw_triangle_filled(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t color);
    void native_ui_draw_bezier_cubic(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, uint32_t color, float thickness, int32_t segments);
    void native_ui_draw_text(float x, float y, uint32_t color, const char* text);
    void native_ui_draw_image(natural_t texture, float min_x, float min_y, float max_x, float max_y, float u0, float v0, float u1, float v1, uint32_t tint);

    /// what is drawn until the pop is cut to the rectangle
    void native_ui_push_clip_rect(float min_x, float min_y, float max_x, float max_y, nbool32 intersect_with_current);
    void native_ui_pop_clip_rect();
    /// pops what scripts pushed and did not pop, and says how many that was. whoever lets
    ///  a script draw calls this when the script is done
    natural_t close_script_clip_rects();

    /// a texture a rendering pipeline holds, by the pipeline's name and the texture's. 0
    ///  when there is none; the size is the texture's, in pixels
    natural_t native_ui_get_pipeline_texture(const char* pipeline, const char* name, int32_t* out_width, int32_t* out_height);
    /// a loaded texture asset, by its id or by the path it was loaded from. what a path led
    ///  to is kept, and a path that leads nowhere is an error in the log once
    natural_t native_ui_get_asset_texture(natural_t asset_id, int32_t* out_width, int32_t* out_height);
    natural_t native_ui_get_asset_texture_by_path(const char* path, int32_t* out_width, int32_t* out_height);
    /// an item of that size showing the texture. no texture leaves the item's space empty
    void native_ui_image(natural_t texture, float width, float height, float u0, float v0, float u1, float v1);

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_UI_DRAW_LIST_BINDINGS_HPP
