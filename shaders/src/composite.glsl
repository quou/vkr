#version 450

#begin VERTEX

#include "pp_vertex.glsl"

#end VERTEX

#begin FRAGMENT

#include "pp_common.glsl"
#include "pp_config.glsl"

layout (set = 1, binding = 0) uniform sampler2D tonemapped_scene;
layout (set = 1, binding = 1) uniform sampler2D bloom;

void main() {
	color = texture(tonemapped_scene, fs_in.uv) + texture(bloom, fs_in.uv) * config.bloom_intensity;
}

#end FRAGMENT
