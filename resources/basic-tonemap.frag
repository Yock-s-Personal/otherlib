#include "shader-modules/basic-textured-quad-fragment.glsl"
#include "shader-modules/color-correction.glsl"

in vec2 frag_tex_coords;
out vec4 frag_color;

void main() {
  /// light less than none is a fault upstream, and the curves below are only right from
  ///  zero up: fed a negative they give back a bright colour, which is how one such fault
  ///  came to look like bands of red and green. it is shown as the black it is
  vec3 hdr = max(get_color(frag_tex_coords), vec3(0.0));
  vec3 exposed = oe_tonemap_exposure(hdr, OE_exposure);
  vec3 mapped = oe_tonemap_aces(exposed);
  vec3 display = oe_linear_to_gamma(mapped, OE_gamma);
  frag_color = vec4(display, 1.0);
}
