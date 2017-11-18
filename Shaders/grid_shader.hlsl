cbuffer cb_viewdata : register(b0) {
	float4x4 world_to_view;
	float4x4 view_to_clip;
};

cbuffer cb_griddata : register(b1) {
	float4 axis1;
	float4 axis1_color;
	float4 axis2;
	float4 axis2_color;
	float4 base;
	float4 plain_color;
	uint num_gridlines;
	float spacing;
};

Texture2D diffuse : register(t0);
SamplerState diffuse_sampler : register(s0);

struct vs_in {
	uint vertex_id : SV_VertexID;
};

struct vs_out {
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
};

static const float4 coefficients[8] = {
	{ +1, +1, 0, 0 },
	{ -1, +1, 0, 0 },
	{ +1, -1, 0, 0 },
	{ -1, -1, 0, 0 },

	{ 0, 0, +1, +1, },
	{ 0, 0, -1, +1, },
	{ 0, 0, +1, -1, },
	{ 0, 0, -1, -1, },
};

vs_out vs_main(vs_in i) {
	vs_out o;

	uint cycle = i.vertex_id / 8;
	uint vert = i.vertex_id % 8;

	float4 co = coefficients[vert];
	float3 pos_world = base.xyz
		+ co.x * axis1.xyz * num_gridlines * spacing
		+ co.y * axis2.xyz * cycle * spacing
		+ co.z * axis2.xyz * num_gridlines * spacing
		+ co.w * axis1.xyz * cycle * spacing;

	if (cycle == 0) {
		o.color = vert < 4 ? axis1_color : axis2_color;
	}
	else {
		o.color = plain_color;
	}

	o.pos = mul(view_to_clip, mul(world_to_view, float4(pos_world, 1)));
	return o;
}

float4 ps_main(vs_out i) : SV_TARGET
{
	return i.color;
}
