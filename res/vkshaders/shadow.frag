#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout     : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "include/defines.inc"

layout(location=2) in vec2 v_texcoord0;

#include "include/material.inc"

#define HYP_SHADOW_SAMPLE_ALBEDO 0

void main()
{
#if HYP_SHADOW_SAMPLE_ALBEDO
    if (HAS_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map)) {
        vec4 albedo_texture = texture(GET_TEXTURE(MATERIAL_TEXTURE_ALBEDO_map), v_texcoord0);
        
        if (albedo_texture.a < MATERIAL_ALPHA_DISCARD) {
            discard;
        }
    }
#endif
}
