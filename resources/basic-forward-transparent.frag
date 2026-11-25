#include "shader-modules/camera.glsl"
#include "shader-modules/basic-material.glsl"
#include "shader-modules/simulation-environment.glsl"
#include "shader-modules/basic-lighting.glsl"
#include "shader-modules/scene-lighting.glsl"

in vec3 world_normal;
in vec3 world_position;

layout (location = 0) out vec4 OE_summed_color;
layout (location = 1) out float OE_summed_cover;

/// forward-lit transparency: the same calculate_lighting as the deferred resolve, run per
/// fragment straight from the material, with authored opacity (material x texture x tint,
/// already folded into the packed base_color). nothing is laid over anything here: every
/// see-through surface over a pixel adds its colour, weighted by how much it covers and
/// by how near it is, and adds to how much of what is behind is covered; a pass after this
/// makes the picture of the two sums. so the order surfaces are drawn in does not matter
void main() {
  material mat = get_instance_material();
  vec4 base = material_base_color();

  vec4 lit = calculate_lighting(base.rgb, world_position, normalize(world_normal), mat.roughness, mat.metalness);
  /// what the surface mirrors is seen however clear it is: glass at a slant is a mirror
  float cover = clamp(mix(base.a, 1.0, lit.a), 0.0, 0.999);

  /// a nearer surface counts for more in the colour, as it would laid over a farther one
  float nearness = 1.0 - gl_FragCoord.z;
  float weight = clamp(cover * max(1e-2, 3e3 * nearness * nearness * nearness), 1e-2, 3e3);
  OE_summed_color = vec4(lit.rgb * cover, cover) * weight;

  /// what is left uncovered multiplies from surface to surface, which as a sum is its log
  OE_summed_cover = -log(1.0 - cover);
}
