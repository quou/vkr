#version 450

#begin VERTEX

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
	vec2 uv;
} vs_out;

void main() {
	vs_out.normal = mat3(push_data.transform) * normal;
	vs_out.world_pos = vec3(push_data.transform * vec4(position, 1.0));
	vs_out.uv = uv;

	gl_Position = data.projection * data.view * push_data.transform * vec4(position, 1.0);
}

#end VERTEX

#begin FRAGMENT

layout (location = 0) out vec4 color;

layout (binding = 1) uniform FragmentData {
	vec3 camera_pos;
} data;

layout (location = 0) in VertexOut {
	vec3 normal;
	vec3 world_pos;
	vec2 uv;
} fs_in;

layout (binding = 2) uniform sampler2D albedo;

void main() {
	vec3 normal = normalize(fs_in.normal);

	const vec3 light_pos = vec3(0.0, 0.0, 5.0);
	const vec3 light_color = vec3(1.0, 1.0, 1.0);
	const float light_range = 10.0;
	const float light_intensity = 1.0;

	const float shininess = 200.0;

	vec3 view_dir = normalize(data.camera_pos - fs_in.world_pos);

	vec3 light_dir = normalize(light_pos - fs_in.world_pos);
	vec3 reflect_dir = reflect(normal, -light_dir);

	float attenuation = 1.0 / (pow(length(light_pos - fs_in.world_pos), 2.0) + 1);

	vec3 diffuse =
		attenuation *
		light_color *
		light_intensity *
		max(dot(light_dir, normal), 0.0);

	vec3 specular =
		attenuation *
		light_color *
		light_intensity *
		pow(max(dot(view_dir, reflect_dir), 0.0), shininess);

	color = texture(albedo, fs_in.uv) * vec4(diffuse + specular, 1.0);
}
#end FRAGMENT
