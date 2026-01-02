Texture2D srcTexture : register(t0);
SamplerState linearSampler : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

[shader("fragment")]
float4 main(VSOutput input) : SV_TARGET0
{
    return srcTexture.Sample(linearSampler, input.uv);
}