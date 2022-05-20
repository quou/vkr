#version 450

#begin VERTEX

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;

layout (binding = 0) uniform VertexBuffer {
	mat4 view;
	mat4 projection;
} data;

layout (push_constant) uniform PushData {
	mat4 transform;
} push_data;

layout (location = 0) out VertexOut {
	mat3 tbn;
	vec3 world_pos;
	vec2 uv;
} vs_out;

void main() {
	vs_out.world_pos = vec3(push_data.transform * vec4(position, 1.0));
	vs_out.uv = uv;

	vec3 t = normalize(vec3(push_data.transform * vec4(tangent, 0.0)));
	vec3 n = normalize(vec3(push_data.transform * vec4(normal, 0.0)));
	vec3 b = normalize(vec3(push_data.transform * vec4(bitangent, 0.0)));

	vs_out.tbn = mat3(t, b, n);

	gl_Position = data.projection * data.view * vec4(vs_out.world_pos, 1.0);
}

#end VERTEX

#begin FRAGMENT

#define max_point_lights 32

struct PointLight {
	float intensity, range;
	vec3 diffuse, specular;
	vec3 position;
};

layout (location = 0) out vec4 color;

layout (binding = 1) uniform FragmentData {
	vec3 camera_pos;
	int point_light_count;
	PointLight point_lights[max_point_lights];
} data;

layout (location = 0) in VertexOut {
	mat3 tbn;
	vec3 world_pos;
	vec2 uv;
} fs_in;

//layout (set = 1, binding = 0) uniform sampler2D diffuse_map;
//layout (set = 1, binding = 1) uniform sampler2D normal_map;

vec3 compute_point_light(vec3 normal, vec3 view_dir, PointLight light) {
	vec3 light_dir = normalize(light.position - fs_in.world_pos);
	vec3 reflect_dir = reflect(light_dir, normal);

	float dist = length(light.position - fs_in.world_pos);

	float attenuation = 1.0 / (pow((dist / light.range) * 5.0, 2.0) + 1);

	vec3 diffuse =
		attenuation *
		light.diffuse *
		light.intensity *
		max(dot(light_dir, normal), 0.0);

	vec3 specular =
		attenuation *
		light.specular *
		light.intensity *
		pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

	return diffuse + specular;
}

void main() {
//	vec3 normal = texture(normal_map, fs_in.uv).rgb * 2.0 - 1.0;
//	normal = normalize(fs_in.tbn * normal);

	vec3 normal = normalize(fs_in.tbn[2]);

	vec3 view_dir = normalize(data.camera_pos - fs_in.world_pos);

	vec3 lighting_result = vec3(0.1);

	for (int i = 0; i < data.point_light_count; i++) {
		lighting_result += compute_point_light(normal, view_dir, data.point_lights[i]);
	}

//	color = texture(diffuse_map, fs_in.uv) * vec4(lighting_result, 1.0);
	color = vec4(lighting_result, 1.0);
}
#end FRAGMENT
