#version 450

#begin VERTEX

#include "pp_vertex.glsl"

#end VERTEX

#begin FRAGMENT

#include "pp_common.glsl"

layout (set = 1, binding = 0) uniform sampler2D input_texture;

/* From: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/ */
vec3 aces(vec3 x) {
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
	vec4 tc = texture(input_texture, fs_in.uv);

	color = vec4(aces(tc.rgb), tc.a);
}

#end FRAGMENT
