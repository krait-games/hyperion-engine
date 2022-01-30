#version 430

in vec4 v_position;
in vec4 ndcPos;


#include "../include/matrices.inc"
#include "../include/frag_output.inc"

layout(binding = 0, rgba32f) uniform image3D framebufferImage;
uniform vec4 C_albedo;
uniform vec3 u_probePos;

uniform sampler2D DiffuseMap;
uniform int HasDiffuseMap;

uniform mat4 StorageTransformMatrix;

uniform float u_intensity;

// output
in GSOutput
{
	vec3 normal;
	vec3 texcoord0;
	vec3 offset;
	vec4 position;
} gs_out;
vec3 worldToTex(vec3 world) 
{
	vec4 ss = u_projMatrix * vec4(world, 1.0);
	ss /= ss.w;
	ss.z = 0.0;
	
	return ss.xyz;

}
void main(void) 
{
	float voxelImageSize = 128.0;
	float halfVoxelImageSize = voxelImageSize * 0.5;

	vec4 storagePos = ndcPos;//StorageTransformMatrix * ndcPos;
    storagePos.xyz /= storagePos.w;	
	
	vec4 imageColor = C_albedo;
	
	//if (HasDiffuseMap == 1) {
	  imageColor = texture(DiffuseMap, gs_out.texcoord0.xy);
	//}
	
	//imageColor *= u_intensity;
	//imageColor *= lighting;
	//imageStore(framebufferImage, ivec3(gl_FragCoord.x/8, gl_FragCoord.y/8, gl_FragCoord.z/8), C_albedo);
	
	ivec3 voxelPos = ivec3(imageSize(framebufferImage) * (storagePos.xyz * .5 + .5));
	vec3 test_store_pos = (gs_out.position).xyz * 0.2;
	imageStore(framebufferImage, ivec3(test_store_pos.x, test_store_pos.y, test_store_pos.z)-ivec3(u_probePos.x, u_probePos.y, u_probePos.z)+ivec3(halfVoxelImageSize), imageColor);
	//imageStore(framebufferImage, voxelPos, imageColor);
}
