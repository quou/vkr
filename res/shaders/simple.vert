#version 450

layout (location = 0) in vec2 position;

layout (binding = 0) uniform MatrixBuffer {
	mat4 transform;
} matrices;

layout (push_constant) uniform Transform {
	mat4 matrix;
} transform;

void main() {
	gl_Position = transform.matrix * vec4(position, 0.0, 1.0);
}
