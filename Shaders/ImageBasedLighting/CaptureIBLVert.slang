cbuffer Constants : register(b0)
{
    column_major float4x4 projView;
    uint skyView;
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
    float3 UVW : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.UVW = input.position;

    output.position = mul(projView, float4(input.position, 1.0f));
    output.position = output.position.xyww; 
    return output;
}
