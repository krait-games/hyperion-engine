#version 450

layout(location=0) out vec3 v_position;
layout(location=1) out vec2 v_texcoord0;
layout(location=2) out vec3 v_light_direction;
layout(location=3) out vec3 v_camera_position;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord0;
layout(location = 3) in vec2 a_texcoord1;
layout(location = 4) in vec3 a_tangent;
layout(location = 5) in vec3 a_bitangent;



layout(std140, set = 2, binding = 0, row_major) uniform SceneDataBlock {
    mat4 view;
    mat4 projection;
    vec4 camera_position;
    vec4 light_direction;
} scene;


void main() {
    vec4 position = vec4(a_position, 1.0);

    v_position = position.xyz;
    v_texcoord0 = a_texcoord0;
    v_light_direction = scene.light_direction.xyz;
    v_camera_position = scene.camera_position.xyz;

    gl_Position = position;
} 