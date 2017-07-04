cbuffer cb_color : register(b0) {
	float4 color;
};

Texture2D diffuse : register(t0);
SamplerState diffuse_sampler : register(s0);

struct vs_out {
	float4 pos : SV_POSITION;
	float4 original_pos : TEXCOORD0;
};

vs_out vs_main(float4 in_pos : POSITION)
{
	vs_out o;
	o.pos = in_pos;
	o.original_pos = in_pos;
    return o;
}

float4 ps_main(vs_out i) : SV_TARGET
{
    return diffuse.Sample(diffuse_sampler, i.original_pos.xy);
}