/**
 * \file scripting/dotnet_bindings/draw_bindings.hpp
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_DRAW_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_DRAW_BINDINGS_HPP

#include <cstdint>

#include "dotnet/types.hpp"

namespace other {
  namespace bindings {

    /// dynamic draw submissions; target is DrawTarget in C#: 0 the depth-tested in-scene
    ///   overlay streams, 1 the debug overlay view (drawn on top of the frame, editor only),
    ///   2 that view again but hidden by what the scene has in front
    void native_draw_line(float ax, float ay, float az, float bx, float by, float bz, float r, float g, float b, float a, uint32_t target);
    void native_draw_triangle(float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz, float r, float g, float b, float a, uint32_t target);
    void native_draw_point(float px, float py, float pz, float r, float g, float b, float a, uint32_t target);

    void native_draw_ray(float ox, float oy, float oz, float dx, float dy, float dz, float len, float r, float g, float b, float a, uint32_t target);
    void native_draw_arrow(float ax, float ay, float az, float bx, float by, float bz, float r, float g, float b, float a, uint32_t target);
    void native_draw_aabb(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z, float r, float g, float b, float a, uint32_t target);
    void native_draw_obb(float m00, float m01, float m02, float m03,
                         float m10, float m11, float m12, float m13,
                         float m20, float m21, float m22, float m23,
                         float m30, float m31, float m32, float m33,
                         float r, float g, float b, float a,
                         uint32_t target);
    void native_draw_sphere(float cx, float cy, float cz, float radius, float r, float g, float b, float a, uint32_t target);
    void native_draw_frustum(float m00, float m01, float m02, float m03,
                             float m10, float m11, float m12, float m13,
                             float m20, float m21, float m22, float m23,
                             float m30, float m31, float m32, float m33,
                             float r, float g, float b, float a,
                             uint32_t target);
    void native_draw_transform(float m00, float m01, float m02, float m03,
                               float m10, float m11, float m12, float m13,
                               float m20, float m21, float m22, float m23,
                               float m30, float m31, float m32, float m33,
                               float scale, uint32_t target);
    /// draws @p object_id's render mesh at an arbitrary transform (tinted/wireframe) - the
    ///  debug-HUD "ghost" primitive; objects without an uploaded model draw nothing
    void native_draw_mesh(uint64_t object_id, float m00, float m01, float m02, float m03,
                          float m10, float m11, float m12, float m13,
                          float m20, float m21, float m22, float m23,
                          float m30, float m31, float m32, float m33,
                          float r, float g, float b, float a,
                          nbool32 wireframe, uint32_t target);

    /// draws the grid_component on @p object_id where the object is this frame; in-scene grids
    ///   render procedurally (spherical falls back to depth-tested lines), debug grids as overlay lines
    void native_draw_grid(uint64_t object_id, uint32_t target);

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_DRAW_BINDINGS_HPP
