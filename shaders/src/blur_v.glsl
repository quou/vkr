#version 450

#begin VERTEX

#include "pp_vertex.glsl"

#end VERTEX

#begin FRAGMENT

#include "pp_common.glsl"
#include "pp_config.glsl"

layout (set = 1, binding = 0) uniform sampler2D input_texture;

void main() {
	float blur_amt = config.bloom_blur_intensity;

	vec4 accum = vec4(0.0);
	float tap_count = 0;

	vec2 correction = 1.0 / textureSize(input_texture, 0);

	for (float x = -0.1; x <= 0.1; x += 0.01) {
		accum += texture(input_texture, fs_in.uv + vec2(0.0f, x) * blur_amt * correction);
		tap_count++;
	}

	color = accum / tap_count;
}

#end FRAGMENT
