#include "ShadowInc.slang"

cbuffer rootConstant : register(b0)
{
    column_major float4x4 localModel;
    column_major float4x4 projViewShadow;
    uint cascadeIndex;
}

//cbuffer shadowMapBuffer : register(b1)
//{
//    float4 lightDirectionIntensity;
//    ColumnMajor projView[SHADOWMAP_CASCADES]; // When using an array it forces it to row major automatically ! might not be the same for all drivers so has to be tested
//    float4 cascadeDistance; // Distances are store per float value
//    float4 cascadeSplitDepthRange;
//};

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
