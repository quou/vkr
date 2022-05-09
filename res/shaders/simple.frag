#version 450

layout (location = 0) out vec4 color;

layout (binding = 1) uniform FragmentData {
	vec3 camera_pos;
} data;

layout (location = 0) in VertexOut {
	vec3 normal;
	vec3 world_pos;
} fs_in;

void main() {
	vec3 normal = normalize(fs_in.normal);

	const vec3 light_pos = vec3(0.0, 0.0, 5.0);
	const vec3 light_color = vec3(1.0, 0.0, 0.0);
	const float light_range = 10.0;
	const float light_intensity = 1.0;

	const float shininess = 32.0;

	vec3 view_dir = normalize(data.camera_pos - fs_in.world_pos);

	vec3 light_dir = normalize(light_pos - fs_in.world_pos);
	vec3 reflect_dir = reflect(-light_dir, normal);

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

	color = vec4(diffuse + specular, 1.0);
}
