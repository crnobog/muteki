cbuffer cb_proj : register(b0) {
	float4x4 proj_matrix;
};

sampler sampler0 : register(s0);
Texture2D texture0 : register(t0);

struct vs_in {
	float2 pos : POSITION;
	float2 uv : TEXCOORD0;
	float4 col : COLOR0;	
};

struct ps_in {
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

ps_in vs_main(vs_in i) {
	ps_in o;
	o.pos = mul(proj_matrix, float4(i.pos.xy, 0.f, 1.f));
	o.col = i.col;
	o.uv = i.uv;
	return o;
}

float4 ps_main(ps_in i) : SV_TARGET {
	float4 out_col = i.col * texture0.Sample(sampler0, i.uv);
	return out_col;
}