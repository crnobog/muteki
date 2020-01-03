#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 diffuse_coord;

layout(location = 0) out vec2 out_diffuse_coord;

layout(binding = 0, row_major) uniform ViewData {
	mat4 world_to_view;
	mat4 view_to_clip;
} view_data;

layout(binding = 1, row_major) uniform InstanceData {
	mat4 model_to_world;
} instance_data;

void main() {
	vec4 pos_world = instance_data.model_to_world * vec4(pos, 1.0f);
	vec4 pos_view = view_data.world_to_view * pos_world;
	
	gl_Position = view_data.view_to_clip * pos_view;
	out_diffuse_coord = diffuse_coord;
}
