/// the edge of what is selected. OE_mask is white where a selected mesh was drawn and
///  black elsewhere; a pixel outside the white with white within the outline's width of
///  it is the outline. everything else is left as the frame has it

uniform sampler2D OE_mask;
uniform vec4 OE_outline_color;
/// in pixels
uniform float OE_outline_width;

in vec2 frag_tex_coords;

out vec4 frag_color;

void main() {
  if (texture(OE_mask, frag_tex_coords).r > 0.5) {
    discard;
  }

  /// sixteen directions, at the full width and at half of it, so that a thin part of the
  ///  selection is not stepped over
  vec2 texel = 1.0 / vec2(textureSize(OE_mask, 0));
  float found = 0.0;
  for (int i = 0; i < 16; ++i) {
    float angle = 6.28318530718 * float(i) / 16.0;
    vec2 reach = vec2(cos(angle), sin(angle)) * texel * OE_outline_width;
    found = max(found, texture(OE_mask, frag_tex_coords + reach).r);
    found = max(found, texture(OE_mask, frag_tex_coords + reach * 0.5).r);
  }
  if (found < 0.5) {
    discard;
  }

  frag_color = OE_outline_color;
}
