#version 450

#begin VERTEX

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 uv;

layout (location = 0) out VertexOut {
	vec2 uv;
} vs_out;

void main() {
	vs_out.uv = uv;

	gl_Position = vec4(position, 0.0, 1.0);
}

#end VERTEX

#begin FRAGMENT

layout (location = 0) out vec4 color;

layout (location = 0) in VertexOut {
	vec2 uv;
} fs_in;

layout (set = 1, binding = 0) uniform sampler2D input_texture;

layout (binding = 0) uniform Config {
	float bloom_threshold;
	float bloom_blur_intensity;
	float bloom_intensity;
	vec2 screen_size;
} config;

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
