#version 450

layout (location = 0) in vec2 position;

layout (binding = 0) uniform VertexBuffer {
	mat4 view;
	mat4 projection;
} data;

void main() {
	gl_Position = data.projection * data.view * vec4(position, 0.0, 1.0);
}
