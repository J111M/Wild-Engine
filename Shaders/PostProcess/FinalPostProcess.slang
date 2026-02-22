cbuffer RootConsant : register(b0)
{
    float2 textureSize;
    uint32_t srcTextureView;
    uint32_t pad;
};

RWTexture2D<float4> outputTexture : register(u0);

Texture2D textures[4096] : register(t0, space0);
SamplerState pointSampler : register(s0);

#define PI 3.14159265358979323846

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x >= textureSize.x || id.y >= textureSize.y) return;
    uint2 coords = id.xy;

    float2 uv = coords / textureSize.xy;

    float4 srcTexture = textures[NonUniformResourceIndex(srcTextureView)].Load(int3(coords, 0));

    float3 finalColor = srcTexture.rgb;
    float lum = dot(finalColor, float3(0.2126, 0.7152, 0.0722));
    finalColor = finalColor / (1.0 + lum);

    outputTexture.Store(coords, float4(float3(finalColor), 1.0f));
}
