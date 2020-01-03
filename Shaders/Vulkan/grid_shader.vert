#version 450

layout(binding = 0, row_major) uniform ViewData {
	mat4 world_to_view;
	mat4 view_to_clip;
} view_data;

layout(binding = 1, row_major) uniform GridData {
	vec4 axis1;
	vec4 axis1_color;
	vec4 axis2;
	vec4 axis2_color;
	vec4 base;
	vec4 plain_color;
	uint num_gridlines;
	float spacing;
} grid_data;

layout(location = 0) out vec4 o_color;

const vec4 coefficients[8] = {
	{ +1, +1, 0, 0 },
	{ -1, +1, 0, 0 },
	{ +1, -1, 0, 0 },
	{ -1, -1, 0, 0 },

	{ 0, 0, +1, +1, },
	{ 0, 0, -1, +1, },
	{ 0, 0, +1, -1, },
	{ 0, 0, -1, -1, },
};

void main() {
	uint cycle = gl_VertexIndex / 8;
	uint vert = gl_VertexIndex % 8;

	vec4 co = coefficients[vert];
	vec3 pos_world = grid_data.base.xyz
		+ co.x * grid_data.axis1.xyz * grid_data.num_gridlines * grid_data.spacing
		+ co.y * grid_data.axis2.xyz * cycle * grid_data.spacing
		+ co.z * grid_data.axis2.xyz * grid_data.num_gridlines * grid_data.spacing
		+ co.w * grid_data.axis1.xyz * cycle * grid_data.spacing;

	if (cycle == 0) {
		o_color = vert < 4 ?grid_data. axis1_color : grid_data.axis2_color;
	}
	else {
		o_color = grid_data.plain_color;
	}

	gl_Position = view_data.view_to_clip * view_data.world_to_view * vec4(pos_world, 1.0f);
}