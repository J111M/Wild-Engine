struct VSOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

[shader("fragment")]
float4 main(VSOutput input) : SV_Target0
{
    return float4(input.color, 1.0f);
}