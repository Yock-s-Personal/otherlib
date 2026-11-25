#include "shader-modules/simulation-environment.glsl"
#include "shader-modules/basic-lighting.glsl"

layout(rgba16f) uniform writeonly image3D OE_voxel_tex;
in vec3 f_world_pos;

void main() {
  /// what is outside the volume along the axis looked down is in nobody's voxel
  vec3 uvw = (f_world_pos - world_min.xyz) / world_extent();
  if (any(lessThan(uvw, vec3(0.0))) || any(greaterThanEqual(uvw, vec3(1.0)))) {
    return;
  }
  imageStore(OE_voxel_tex, ivec3(uvw * vec3(imageSize(OE_voxel_tex))), vec4(1.0));
}
