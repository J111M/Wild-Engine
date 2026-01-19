cbuffer Constants : register(b0)
{
    uint skyView;
};

cbuffer matrices : register(b1)
{
    column_major float4x4 view;
    column_major float4x4 projection;
}

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
    float3 UVW : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.UVW = input.position;

    float4x4 projectionView = mul(projection, view);

    output.position = mul(projectionView, float4(input.position, 1.0f));
    output.position = output.position.xyww; 
    return output;
}
