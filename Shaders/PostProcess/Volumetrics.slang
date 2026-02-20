cbuffer RootConsant : register(b0)
{
    float2 textureSize;
    float2 nearFar;
    float aspect;
    uint srcTextureView;
    uint depthView;
};

cbuffer SceneData : register(b1)
{
    column_major float4x4 inverseProj;
    column_major float4x4 inverseView;
    float3 cameraPosition;
};

RWTexture2D outputTexture : register(u0);

Texture2D textures[4096] : register(t0, space0);
TextureCube textureCubes[4096] : register(t0, space1);
SamplerState pointSampler : register(s0);

float4 GetWorldSpacePosition(float3 position)
{
    float4 positionVS = mul(inverseProj, float4(position, 1.0));
    positionVS /= positionVS.w;

    return mul(inverseView, positionVS);
}

float LinearizeDepth(float depth)
{
    return (nearFar.x * nearFar.y) / (nearFar.y - depth * (nearFar.y - nearFar.x));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x > textureSize.x || id.y > textureSize.y) return;
    uint2 coords = id.xy;

    float2 uv = coords / textureSize.xy;

    float4 srcTexture = textures[NonUniformResourceIndex(srcTextureView)].Load(int3(coords, 0));

    float depth = textures[NonUniformResourceIndex(depthView)].Load(int3(coords, 0)).r;
    float2 ndc = (float2(coords) + 0.5) / float2(aspect) * 2.0 - 1.0;
    //ndc.y = -ndc.y;

    depth = LinearizeDepth(depth);

    float4 position = GetWorldSpacePosition(float3(ndc, depth));

    float4 color = srcTexture * position;

    outputTexture.Store(coords, color);
}