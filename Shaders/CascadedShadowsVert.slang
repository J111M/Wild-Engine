#include "ShadowInc.slang"

cbuffer rootConstant : register(b0)
{
    column_major float4x4 localModel;
    column_major float4x4 projViewShadow;
    uint cascadeIndex;
}

cbuffer shadowMapBuffer : register(b1)
{
    float4 lightDirectionIntensity;
    column_major float4x4 projView[SHADOWMAP_CASCADES];
    float4 cascadeDistance; // Distances are store per float value
};

struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
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
    output.position = mul(projViewShadow, mul(localModel, float4(input.position, 1.0f)));
    output.color = input.color;
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}