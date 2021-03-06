#ifndef HYP_GBUFFER_GLSL
#define HYP_GBUFFER_GLSL

#include "defines.inc"

#define NUM_GBUFFER_TEXTURES 6
#define HYP_GBUFFER_FLIP_Y 0

#include "shared.inc"
#include "packing.inc"

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 0) uniform texture2D gbuffer_textures[NUM_GBUFFER_TEXTURES];
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 1) uniform texture2D gbuffer_depth_texture;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 2) uniform texture2D gbuffer_mip_chain;
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 3) uniform sampler   gbuffer_depth_sampler; // nearest
layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 4) uniform sampler   gbuffer_sampler;       // mipmap linear

#define gbuffer_albedo_texture     (gbuffer_textures[0])
#define gbuffer_normals_texture    (gbuffer_textures[1])
#define gbuffer_positions_texture  (gbuffer_textures[2])
#define gbuffer_material_texture   (gbuffer_textures[3])
#define gbuffer_tangents_texture   (gbuffer_textures[4])
#define gbuffer_bitangents_texture (gbuffer_textures[5])

layout(set = HYP_DESCRIPTOR_SET_GLOBAL, binding = 5) uniform sampler2D gbuffer_deferred_result;

vec4 SampleGBuffer(texture2D tex, vec2 texcoord)
{
    return Texture2D(gbuffer_depth_sampler, tex, texcoord);
}

#endif
