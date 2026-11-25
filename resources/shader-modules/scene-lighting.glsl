/// canonical scene lighting for material-driven passes: the deferred shading resolve and the
/// forward transparent pass compile this against the same light/environment data, so surfaces
/// shade identically on both paths. flat include convention (stb_include has no guards) - the
/// includer pulls in camera.glsl, simulation-environment.glsl, and basic-lighting.glsl first.

uniform sampler3D OE_env_cubemap;
uniform sampler3D OE_voxel_tex;

#ifndef OE_SHADOW_MAX_STEPS
#define OE_SHADOW_MAX_STEPS 48
#endif

#ifndef OE_SHADOW_NORMAL_BIAS
#define OE_SHADOW_NORMAL_BIAS 1.5   // start offset along the surface normal, in voxels (anti-acne)
#endif

vec3 oe_environment_ambient_color(vec3 amb_col, vec3 N) {
  float up = clamp(normalize(N).y * 0.5 + 0.5, 0.0, 1.0);
  return mix(ground_color.rgb, amb_col, up);
}

vec3 oe_environment_ambient(vec3 world_pos, vec3 N) {
  vec3  amb_up = texture(OE_env_cubemap, oe_world_to_volume(world_pos)).rgb;
  return oe_environment_ambient_color(amb_up, N);
}

float oe_point_shadow(vec3 surface_pos, vec3 surface_normal, vec3 light_pos) {
  vec3 to_light = light_pos - surface_pos;
  float dist = length(to_light);
  if (dist < 1e-4) {
    return 1.0;
  }
  vec3  dir = to_light / dist;

  ivec3 dim = textureSize(OE_voxel_tex, 0);
  vec3  vsize = oe_voxel_size(dim);                       // world units per voxel
  float vlen = max(max(vsize.x, vsize.y), vsize.z);

  vec3 p = surface_pos + surface_normal * (vlen * OE_SHADOW_NORMAL_BIAS) + dir * (vlen * 0.5);
  float remaining = dist - vlen * (OE_SHADOW_NORMAL_BIAS + 1.0);
  if (remaining <= 0.0) {
    return 1.0;
  }

  int steps = int(clamp(remaining / vlen, 1.0, float(OE_SHADOW_MAX_STEPS)));
  float step_len = remaining / float(steps);

  float trans = 1.0;
  for (int i = 0; i < steps; ++i) {
    float occ = texture(OE_voxel_tex, oe_world_to_voxel(p, dim)).r;
    trans *= (1.0 - clamp(occ, 0.0, 1.0));
    if (trans < 0.01) {
      return 0.0;
    }
    p += dir * step_len;
  }
  return trans;
}

const float OE_PI = 3.14159265359;

/// the three parts of the usual highlight off a rough surface (GGX), with a the roughness
/// squared: how many of the surface's facets face halfway between light and eye, how many
/// of those are not hidden by their neighbours, and how much a facet mirrors at that angle
float oe_ggx_distribution(float n_h, float a) {
  float a2 = a * a;
  float d = n_h * n_h * (a2 - 1.0) + 1.0;
  return a2 / (OE_PI * d * d);
}

float oe_ggx_visibility(float n_l, float n_v, float a) {
  return 0.5 / max(mix(2.0 * n_l * n_v, n_l + n_v, a), 1e-4);
}

vec3 oe_fresnel(vec3 f0, float cosine) {
  return f0 + (1.0 - f0) * pow(clamp(1.0 - cosine, 0.0, 1.0), 5.0);
}

/// what a surface sends to the eye of the light one light gives it. a light's colour is
/// what a white matt surface facing it shows, so the body takes it whole and the highlight
/// is scaled to keep its share
vec3 oe_light_response(vec3 n, vec3 v, vec3 l, vec3 radiance, vec3 body, vec3 f0, float a) {
  float n_l = dot(n, l);
  if (n_l <= 0.0) {
    return vec3(0.0);
  }
  vec3 h = normalize(l + v);
  float n_v = max(dot(n, v), 1e-3);
  vec3 highlight = oe_ggx_distribution(max(dot(n, h), 0.0), a) * oe_ggx_visibility(n_l, n_v, a) * oe_fresnel(f0, max(dot(v, h), 0.0));
  return (body + highlight * OE_PI) * radiance * n_l;
}

/// rgb is the light a surface sends to the eye; a is how much of that is mirrored sky,
/// which a see-through surface shows however thin it is
vec4 calculate_lighting(vec3 diffuse, vec3 world_position, vec3 world_normal, float roughness, float metalness) {
  vec3 n = normalize(world_normal);
  vec3 v = normalize(camera_position.xyz - world_position);

  /// roughness is roughness: 0 a mirror, 1 chalk. a metal has no colour of its own under
  /// its shine and mirrors in its colour; anything else mirrors a little of everything
  float rough = clamp(roughness, 0.08, 1.0);
  float a = rough * rough;
  vec3 body = diffuse * (1.0 - metalness);
  vec3 f0 = mix(vec3(0.04), diffuse, metalness);

  vec3 direct = vec3(0.0);
  for (int i = 0; i < OE_num_lights; ++i) {
    if (lights[i].type == 1.f) {
      vec3 to_light = lights[i].vector.xyz - world_position;
      float dist = max(length(to_light), 1e-4);
      direct += oe_light_response(n, v, to_light / dist, lights[i].color.rgb * attenuate(dist), body, f0, a);
    } else if (lights[i].type == 2.f) {
      /// directional (gpu::light::kDirection): vector.xyz points TOWARD the light, with no
      /// falling off and no shadow
      direct += oe_light_response(n, v, normalize(lights[i].vector.xyz), lights[i].color.rgb, body, f0, a);
    }
  }

  /// the sun is the environment's, and the one light with a shadow map
  float in_sun = 1.0 - calculate_direction_light_shadow(world_position, n);
  direct += in_sun * oe_light_response(n, v, normalize(sun_direction.xyz), sun_color.rgb * sun_color.w, body, f0, a);

  /// the sky lights what the sun does not reach: its light from all round on the body,
  /// dimmed by what stands over the place, and its mirror image in the shine, smeared by
  /// the roughness. a place shaded from the sun still has the sky
  vec4 env = texture(OE_env_cubemap, oe_world_to_volume(world_position));
  vec3 sky_light = oe_environment_ambient_color(env.rgb, n) * env.a;
  vec3 mirrored = oe_sky_color(reflect(-v, n), rough) * world_max.w * mix(1.0, env.a, 0.5);
  vec3 mirror = f0 + (max(vec3(1.0 - rough), f0) - f0) * pow(1.0 - max(dot(n, v), 0.0), 5.0);

  return vec4(direct + body * sky_light + mirror * mirrored, (mirror.r + mirror.g + mirror.b) / 3.0);
}
