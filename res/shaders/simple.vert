#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;

layout (binding = 0) uniform VertexBuffer {
	mat4 view;
	mat4 projection;
} data;

layout (push_constant) uniform PushData {
	mat4 transform;
} push_data;

layout (location = 0) out VertexOut {
	vec3 normal;
	vec3 world_pos;
} vs_out;

void main() {
	vs_out.normal = normal;
	vs_out.world_pos = vec3(push_data.transform * vec4(position, 1.0));

	gl_Position = data.projection * data.view * vec4(vs_out.world_pos, 1.0);
}
