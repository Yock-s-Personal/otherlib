layout(std140) uniform simulation_environment_buffer {
  /// unit vector TO sun, w = angular radius
  vec4 sun_direction;
  // rgb = radiance, w = intensity scale
  vec4 sun_color;
  vec4 ambient_color;
  // rgb, w = turbidity
  vec4 zenith_color;
  vec4 horizon_color;
  vec4 ground_color;
  vec4 world_min;  //< xyz = AABB min
  vec4 world_max;  //< xyz = AABB max, w = exposure
};

vec3 world_extent() {
  return world_max.xyz - world_min.xyz;
}

/// the sky's colour along a direction, without the sun. spread is how far the look is
/// smeared: 0 is the sky as it is seen, 1 so far that only up and down are left of it,
/// which is what a rough surface mirrors. the line between sky and ground is never a
/// step, which on a faceted thing showed as one facet light and the next dark
vec3 oe_sky_color(vec3 dir, float spread) {
  float up = normalize(dir).y;
  float climb = mix(pow(clamp(up, 0.0, 1.0), 0.5), 0.5, spread);
  vec3 sky = mix(horizon_color.rgb, zenith_color.rgb, climb);

  /// the ground lightens towards the horizon as the sky does
  float near_horizon = pow(1.0 - clamp(-up, 0.0, 1.0), 8.0) * (1.0 - spread);
  vec3 ground = mix(ground_color.rgb, horizon_color.rgb, 0.5 * near_horizon);

  float edge = mix(0.01, 1.0, spread);
  return mix(ground, sky, smoothstep(-edge, edge, up));
}

/// the sky as the background shows it: its colour, and the sun's disk with its glare
vec3 oe_sky_radiance(vec3 dir) {
  // sun_direction.w = angular radius in radians
  float cd = dot(normalize(dir), normalize(sun_direction.xyz));
  float radius = max(sun_direction.w, 1e-3);
  float disk = smoothstep(cos(radius * 2.0), cos(radius), cd);
  float glare = pow(max(cd, 0.0), 180.0) * 0.25;

  return oe_sky_color(dir, 0.0) + sun_color.rgb * sun_color.w * (disk * 8.0 + glare);
}

vec3 oe_sky_irradiance_up(vec3 world_pos) {
  /// how high in the world, by the volume's height but from the world's own zero: measured
  /// from the volume's floor it would change as the volume follows the camera up and down
  float h = clamp(0.5 + world_pos.y / max(world_extent().y, 1e-3), 0.0, 1.0);
  
  vec3  dome = mix(horizon_color.rgb, zenith_color.rgb, 0.5 + 0.5 * h);
  vec3  sun  = sun_color.rgb * sun_color.w * max(sun_direction.y, 0.0) * (1.0 - cos(sun_direction.w));
  vec3  haze = horizon_color.rgb * (1.0 - h) * 0.25;
  return (dome + sun + haze);
}

vec3 oe_world_to_volume(vec3 p) {
  return clamp((p - world_min.xyz) / world_extent(), vec3(0.0), vec3(1.0));
}

ivec3 oe_world_to_voxel(vec3 world_pos, ivec3 dim) {
  return ivec3(oe_world_to_volume(world_pos) * vec3(dim));
}

vec3 oe_voxel_to_world(ivec3 voxel, ivec3 dim) {
  vec3 uvw = (vec3(voxel) + 0.5) / vec3(dim);
  return world_min.xyz + uvw * world_extent();
}

vec3 oe_voxel_size(ivec3 dim) {
  return world_extent() / vec3(dim);
}