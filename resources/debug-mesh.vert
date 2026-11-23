#include "shader-modules/camera.glsl"

layout(location = 0) in vec3 OE_position;   // model-space position attribute of the mesh
/// the vertex stream is all floats: bone ids arrive as floats (see basic-geometry.glsl)
layout(location = 5) in vec4 OE_bone_ids;
layout(location = 6) in vec4 OE_bone_weights;

uniform mat4 OE_model;
uniform vec4 OE_tint;
/// the pose of the object the mesh is drawn for. OE_use_bones is 0 for a mesh that has
///  none, which is drawn as it was modelled
uniform mat4 OE_bones[MAX_BONES];
uniform int OE_use_bones;

layout(location = 0) out vec4 OE_vert_color;

mat4 oe_bone_transform() {
  if (OE_use_bones != 1) {
    return mat4(1.0);
  }

  mat4 bone_transform = mat4(0.0);
  float weight = 0.0;
  for (int i = 0; i < MAX_VERTEX_BONE_INFLUENCE; ++i) {
    int id = int(OE_bone_ids[i]);
    if (id < 0 || id >= MAX_BONES) {
      continue;
    }
    bone_transform += OE_bones[id] * OE_bone_weights[i];
    weight += OE_bone_weights[i];
  }

  /// the whole mesh is drawn at once, its unrigged parts too: a vertex no bone moves stays
  ///  where it was modelled instead of collapsing through mat4(0)
  return weight > 0.0 ? bone_transform : mat4(1.0);
}

void main() {
  OE_vert_color = OE_tint;
  gl_Position = get_camera_matrix() * OE_model * oe_bone_transform() * vec4(OE_position, 1.0);
}
