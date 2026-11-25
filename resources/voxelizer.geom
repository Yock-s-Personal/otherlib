#include "shader-modules/simulation-environment.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(rgba16f) uniform writeonly image3D OE_voxel_tex;

in  vec3 g_world_pos[];
out vec3 f_world_pos;

/// a triangle smaller than a voxel can fall between the places the rasteriser looks at and
/// leave nothing behind it, and a figure is made of such triangles. its corners and its
/// middle are marked here, which is all there is of a triangle that small; one larger is
/// filled in by the fragments
void oe_mark_voxel(vec3 world_pos) {
  vec3 uvw = (world_pos - world_min.xyz) / world_extent();
  if (any(lessThan(uvw, vec3(0.0))) || any(greaterThanEqual(uvw, vec3(1.0)))) {
    return;
  }
  imageStore(OE_voxel_tex, ivec3(uvw * vec3(imageSize(OE_voxel_tex))), vec4(1.0));
}

void main() {
  oe_mark_voxel(g_world_pos[0]);
  oe_mark_voxel(g_world_pos[1]);
  oe_mark_voxel(g_world_pos[2]);
  oe_mark_voxel((g_world_pos[0] + g_world_pos[1] + g_world_pos[2]) / 3.0);

  // world-space face normal
  vec3 n = abs(cross(g_world_pos[1] - g_world_pos[0], g_world_pos[2] - g_world_pos[0]));
  int axis = (n.x >= n.y && n.x >= n.z) ?
      0 : (n.y >= n.z ? 1 : 2);

  for (int i = 0; i < 3; ++i) {
    f_world_pos = g_world_pos[i];
    /// not held inside the volume: what is outside it is clipped away, where holding it
    /// in would draw it flat against the volume's side
    vec3 uvw = (g_world_pos[i] - world_min.xyz) / world_extent();
    vec2 p = (axis == 0) ?
      uvw.yz : (axis == 1) ?
      uvw.xz : uvw.xy;

    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
    EmitVertex();
  }

  EndPrimitive();
}
