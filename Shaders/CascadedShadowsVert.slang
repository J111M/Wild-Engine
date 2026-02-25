#include "ShadowInc.slang"

cbuffer rootConstant : register(b0)
{
    column_major float4x4 localModel;
    uint cascadeIndex;
}

cbuffer shadowMapBuffer : register(b1)
{
    float4 lightDirectionIntensity;
    column_major float4x4 viewProj[SHADOWMAP_CASCADES];
    float4 cascadeDistance;
};

struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(viewProj[cascadeIndex], mul(localModel, float4(input.position, 1.0f)));
    output.color = input.color;
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}