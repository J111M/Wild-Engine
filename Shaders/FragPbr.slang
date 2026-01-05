struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

cbuffer Constants : register(b0)
{
    column_major float4x4 model;
    uint4 texIndex;
};

Texture2D textures[4096] : register(t0, space0);
SamplerState anisotropySampler : register(s0);

[shader("fragment")]
float4 main(PSInput input) : SV_TARGET
{
    float4 albedoColor = float4(0, 0, 0, 0);
    
    if(texIndex.x != 0)
    {
        Texture2D albedo = textures[NonUniformResourceIndex(texIndex.x)];
        albedoColor = albedo.Sample(anisotropySampler, input.uv);
    }

   return float4(albedoColor.xyz, 1.0f);
   // return float4(float3(1,1,0), 1.0f);
}