#version 450

#begin VERTEX

layout (location = 0) in vec3 position;

layout (binding = 0) uniform VertexBuffer {
	mat4 view;
	mat4 projection;
} data;

layout (push_constant) uniform PushData {
	mat4 transform;
} push_data;

void main() {
	gl_Position = data.projection * data.view * push_data.transform * vec4(position, 1.0);
}

#end VERTEX

#begin FRAGMENT

void main() {}

#end FRAGMENT
