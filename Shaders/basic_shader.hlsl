cbuffer cb_color : register(b0) {
	float4 color;
};

float4 vs_main(float4 in_pos : POSITION) : SV_POSITION
{
    return in_pos;
}

float4 ps_main(float4 in_pos : SV_POSITION) : SV_TARGET
{
    return color;
}