#version 450
#extension GL_ARB_separate_shader_objects : require
#extension GL_EXT_nonuniform_qualifier    : require
#extension GL_EXT_scalar_block_layout     : require

#include "include/defines.inc"

layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_texcoord0;

layout(location=0) out vec4 gbuffer_albedo;
layout(location=1) out vec4 gbuffer_normals;
layout(location=2) out vec4 gbuffer_positions;
layout(location=3) out vec4 gbuffer_material;
layout(location=4) out vec4 gbuffer_tangents;
layout(location=5) out vec4 gbuffer_bitangents;

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 25) uniform textureCube rendered_cubemaps[];

#include "include/gbuffer.inc"
#include "include/material.inc"
#include "include/packing.inc"

void main() {
    vec3 normal = normalize(v_normal);

#if defined(HYP_MATERIAL_CUBEMAP_TEXTURES) && HYP_MATERIAL_CUBEMAP_TEXTURES
    gbuffer_albedo     = vec4(SAMPLE_TEXTURE_CUBE(MATERIAL_TEXTURE_ALBEDO_map, v_position).rgb, 0.0 /* just for now to tell deferred to not perform lighting */);
#else
    gbuffer_albedo     = vec4(0.0);
#endif

    gbuffer_normals    = vec4(0.0);  // not needed
    gbuffer_positions  = vec4(0.0);  // not needed
    gbuffer_material   = vec4(0.0);
    gbuffer_tangents   = vec4(0.0);  // not needed
    gbuffer_bitangents = vec4(0.0);  // not needed
}
