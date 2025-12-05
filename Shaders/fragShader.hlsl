struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

Texture2D albedo : register(t0);
SamplerState anisotropySampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    float4 albedoColor = albedo.Sample(anisotropySampler, input.uv);
    
    return float4(albedoColor.xyz, 1.0f);
}