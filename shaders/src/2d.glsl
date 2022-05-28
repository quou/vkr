#version 450

#begin VERTEX

layout (location = 0) in vec2 position;
layout (location = 1) in vec4 color;
layout (location = 2) in vec2 uv;
layout (location = 3) in float use_texture;

layout (location = 0) out VertexOut {
	vec2 uv;
	vec4 color;
	float use_texture;
} vs_out;

layout (binding = 0) uniform UniformData {
	mat4 projection;
} data;

void main() {
	vs_out.uv = uv;
	vs_out.color = color;
	vs_out.use_texture = use_texture;

	gl_Position = data.projection * vec4(position, 0.0, 1.0);
}

#end VERTEX

#begin FRAGMENT

layout (location = 0) out vec4 color;

layout (location = 0) in VertexOut {
	vec2 uv;
	vec4 color;
	float use_texture;
} fs_in;

layout (binding = 1) uniform sampler2D atlas;

void main() {
	vec4 tex_color = vec4(1.0);
	if (fs_in.use_texture > 0.0) {
		tex_color = texture(atlas, fs_in.uv);
	}

	color = tex_color * fs_in.color;
}

#end FRAGMENT
