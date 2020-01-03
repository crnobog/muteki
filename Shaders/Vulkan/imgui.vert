#version 450

layout(binding = 0, row_major) uniform UBProj {
	mat4 proj_matrix;
} ub_proj;

layout(location = 0) in vec2 i_pos;
layout(location = 1) in vec2 i_uv;
layout(location = 2) in vec4 i_color;

layout(location = 0) out vec4 o_color;
layout(location = 1) out vec2 o_uv;

void main() {
	gl_Position = ub_proj.proj_matrix * vec4(i_pos, 0.0f, 1.0f);
	//gl_Position = vec4(i_pos, 0.0f, 1.0f) * ub_proj.proj_matrix;
	o_color = i_color;
	o_uv = i_uv;
}
