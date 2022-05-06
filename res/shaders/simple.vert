#version 450

layout (location = 0) in vec2 position;

layout (location = 0) uniform MatrixBuffer {
	mat4 transform;
} matrices;

void main() {
	gl_Position = transform * vec4(position, 0.0, 1.0);
}
