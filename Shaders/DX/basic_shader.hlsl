cbuffer cb_viewdata : register(b0) {
	float4x4 world_to_view;
	float4x4 view_to_clip;
};

cbuffer cb_instancedata : register(b1) {
	float4x4 model_to_world;
};

Texture2D diffuse : register(t0);
SamplerState diffuse_sampler : register(s0);

struct vs_out {
	float4 pos : SV_POSITION;
	float2 diffuse_coord : TEXCOORD0;
};

struct vs_in {
	float3 pos : POSITION0;
	float3 normal : NORMAL0;
	float2 diffuse_coord : TEXCOORD0;
};

vs_out vs_main(vs_in vertex) {
	vs_out o;
	float4 pos_world = mul(model_to_world, float4(vertex.pos, 1.0f));
	float4 pos_view = mul(world_to_view, pos_world);
	o.pos = mul(view_to_clip, pos_view);
	o.diffuse_coord = vertex.diffuse_coord;
	return o;
}

float4 ps_main(vs_out i) : SV_TARGET
{
	return diffuse.Sample(diffuse_sampler, i.diffuse_coord);// * color;
}
