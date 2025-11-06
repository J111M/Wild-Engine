struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

cbuffer Constants : register(b0)
{
    float4x4 model;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(model, float4(input.position, 1.0f));
    return output;
}