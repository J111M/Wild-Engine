cbuffer RootConsant : register(b0)
{
    float2 textureSize;
    uint srcTextureView;
    uint depthView;
};

RWTexture2D outputTexture : register(u0);

Texture2D textures[4096] : register(t0, space0);
TextureCube textureCubes[4096] : register(t0, space1);
SamplerState pointSampler : register(s0);

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x > textureSize.x || id.y > textureSize.y)
        return;

    float2 uv = id.xy / textureSize.xy;

    float4 srcTexture = textures[NonUniformResourceIndex(srcTextureView)].Sample(pointSampler, uv);

    float4 color = srcTexture * float4(uv, uv);

    outputTexture.Store(id.xy, color);
}