struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    nointerpolation uint instanceId : INSTANCE_ID;
};

StructuredBuffer<float4> probeIrradiance : register(t1);

[shader("fragment")]
float4 main(VSOutput input) : SV_Target0
{
    float3 lightDirection = normalize(float3(0.4f, 0.8f, 0.25f));
    float shade = 0.35f + 0.65f * saturate(dot(normalize(input.normal), lightDirection));

    float3 irradiance = max(probeIrradiance[input.instanceId].rgb, float3(0.02f, 0.02f, 0.02f));

    return float4(irradiance * shade, 1.0f);
}
