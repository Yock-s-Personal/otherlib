struct light {
  vec4 vector;
  vec4 color;
  float type; // gpu::light constants: 1 - point, 2 - directional, 3 - spot
};

layout (std430) readonly buffer light_buffer {
  light lights[];
};

uniform mat4 OE_light_space_matrix;
uniform vec3 OE_light_position;
uniform vec3 OE_sun_dir;
uniform int OE_num_lights;

uniform sampler2D OE_shadow_map;

float attenuate(float dist){ 
  dist *= DIST_FACTOR; 
  return 1.0f / (CONSTANT + LINEAR * dist + QUADRATIC * dist * dist); 
}

vec3 calc_direction_light(light light, vec3 diffuse_color, vec3 world_normal) {
  vec3 light_dir = normalize(-light.vector.xyz);
  return light.color.rgb * max(dot(world_normal, light_dir), 0.0) * diffuse_color;
}

vec3 calc_point_light(const light light, const vec3 world_normal, const vec3 world_position) {
  const vec3 direction = normalize(light.vector.xyz - world_position);
	const float dist_to_light = distance(light.vector.xyz, world_position);
	return max(dot(normalize(world_normal), direction), 0.0f) * 5 * attenuate(dist_to_light) * light.color.rgb;
}

/// how much of the sun a place is hidden from, 0 to 1
float calculate_direction_light_shadow(vec3 world_position, vec3 world_normal) {
  vec3 normal = normalize(world_normal);
  float facing = clamp(dot(normal, normalize(OE_sun_dir)), 0.0, 1.0);

  /// the place is looked up a little off its own surface, by about a texel of the map
  /// and more the more the surface slants from the sun: a surface must not shade itself.
  /// how wide a texel is in the world is read off the matrix, whose rows are of length
  /// one over half the map's width
  vec2 texel = 1.0 / vec2(textureSize(OE_shadow_map, 0));
  float world_per_texel = 2.0 * texel.x / length(vec3(OE_light_space_matrix[0][0], OE_light_space_matrix[1][0], OE_light_space_matrix[2][0]));
  vec3 lifted = world_position + normal * world_per_texel * (1.0 + 2.0 * (1.0 - facing));

  vec4 light_space_position = OE_light_space_matrix * vec4(lifted, 1.0);
  vec3 proj_coords = light_space_position.xyz / light_space_position.w * 0.5 + 0.5;

  /// off the map there is nothing to say a place is shaded
  if (proj_coords.z > 1.0 || any(lessThan(proj_coords.xy, vec2(0.0))) || any(greaterThan(proj_coords.xy, vec2(1.0)))) {
    return 0.0;
  }

  const float bias = 0.0003;
  float shadow = 0.0;
  for (int x = -2; x <= 2; ++x) {
    for (int y = -2; y <= 2; ++y) {
      float pcf_depth = texture(OE_shadow_map, proj_coords.xy + vec2(x, y) * texel).r;
      shadow += proj_coords.z - bias > pcf_depth ? 1.0 : 0.0;
    }
  }
  return shadow / 25.0;
}