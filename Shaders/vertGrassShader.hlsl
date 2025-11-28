struct VSInput
{
    float3 position : POSITION;
    float texCoord : COORDS;
    float sway : SWAY;
    uint instanceID : SV_InstanceID;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float sway : SWAYIN;
};

cbuffer Constants : register(b0)
{
    matrix model;
    uint bladeId;
    uint foo1;
    uint foo2;
    uint foo3;
};

struct GrassData
{
    float3 position;
};

StructuredBuffer<GrassData> grassBuffer : register(t0);

VSOutput main(VSInput input)
{
    VSOutput output;
    float3 bladePosition = input.position + grassBuffer[input.instanceID].position;
    output.position = mul(model, float4(bladePosition, 1.0f));
    output.sway = input.sway;
    return output;
}