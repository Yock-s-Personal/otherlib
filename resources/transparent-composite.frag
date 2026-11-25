in vec2 frag_tex_coords;

out vec4 frag_color;

uniform sampler2D OE_scene;
uniform sampler2D OE_summed_color;
uniform sampler2D OE_summed_cover;

/// the solid scene with the see-through surfaces over it: their colour is the weighted
/// mean of what each added, and they hide of the scene what together they cover
void main() {
  vec3 scene = texture(OE_scene, frag_tex_coords).rgb;
  vec4 summed = texture(OE_summed_color, frag_tex_coords);
  float covered = 1.0 - exp(-texture(OE_summed_cover, frag_tex_coords).r);

  vec3 seen_through = summed.rgb / max(summed.a, 1e-5);
  frag_color = vec4(mix(scene, seen_through, covered), 1.0);
}
