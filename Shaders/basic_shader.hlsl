
float4 vs_main(float4 in_pos : POSITION) : SV_POSITION
{
    return in_pos;
}

float4 ps_main(float4 in_pos : SV_POSITION) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}