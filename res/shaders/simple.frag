#version 450

layout (location = 0) out vec4 color;

layout (push_constant) uniform Colors {
	layout(offset = 64) vec3 color;
} colors;

void main() {
	color = vec4(colors.color, 1.0);
}
