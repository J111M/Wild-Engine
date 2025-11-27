struct VSInput
{
    float3 position : POSITION;
    float texCoord : COORDS;
    float sway : SWAY;

};

struct VSOutput
{
    float4 position : SV_POSITION;
    float sway : SWAYIN;
};

cbuffer Constants : register(b0)
{
    matrix model;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(model, float4(input.position, 1.0f));
    output.sway = input.sway;
    return output;
}