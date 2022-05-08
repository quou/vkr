#version 450

layout (location = 0) out vec4 color;

layout (binding = 1) uniform ColorBuffer {
	vec3 color;
} colors;

void main() {
	color = vec4(colors.color, 1.0);
}
