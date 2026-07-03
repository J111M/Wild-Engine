struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
};

[shader("fragment")]
float4 main(VSOutput input) : SV_Target0
{
    float3 lightDirection = normalize(float3(0.4f, 0.8f, 0.25f));
    float shade = 0.35f + 0.65f * saturate(dot(normalize(input.normal), lightDirection));

    return float4(float3(1.0f, 0.8f, 0.8f) * shade, 1.0f);
}
