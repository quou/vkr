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

void main() {
	vec4 tc = texture(input_texture, fs_in.uv);

	float pixel_brightness = dot(tc.rgb, vec3(0.2126, 0.7152, 0.0722));

	if (pixel_brightness > 2.0) {
		color = tc;
	} else {
		color = vec4(0.0, 0.0, 0.0, 1.0);
	}
}

#end FRAGMENT
