struct PSInput
{
    float4 position : SV_POSITION;
    float sway : SWAYIN;
};

float4 main(PSInput input) : SV_TARGET
{
    return float4(float3(0.0,input.sway + 0.6,0.0), 1.0f);
}