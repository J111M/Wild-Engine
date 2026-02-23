#include "../Helper/Rng.slang"

struct GrassBlade
{
    float3 worldPosition;
    float rotation; // in radians
    float height;
};

RWStructuredBuffer<GrassBlade> GrassData : register(u0);

cbuffer RootConsant : register(b0)
{
    float4x4 modelMatrix;
    float2 chunkPosition;
    float2 minMaxHeight;
    uint seed;
    uint chunkId;
    uint heightMapView;
};

Texture2D textures[4096] : register(t0, space0);
SamplerState anisotropySampler : register(s0);

float GetTerrainHeight(float2 position, float maxBladePos) {
    float terrainHeight = 0.0f;
    if (heightMapView > 0) {

        float2 uv = float2(position.xy) / maxBladePos;
        terrainHeight = textures[heightMapView].SampleLevel(anisotropySampler, uv, 0).r;
    }

    return terrainHeight;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // TODO add bounds for safity

    // 25000 blades per chunk
    const uint chunkOffset = chunkId * 25000;
    uint offsetId = chunkOffset + DTid.x;

    const float maxBladePosition = 128;
    float2 rHash1;
    float terrainHeight;
    float slopeAngle;
    float elevation = 0.0f;
    int attempt = 0;
    const int maxAttempts = 80;

    // Keep checking for a position that is not on a slope or sand/water level
    do {
        rHash1 = HashFloat2InRange(offsetId * maxAttempts + attempt, 0, maxBladePosition);

        // Check slope position
        float step = 1.0; // tune this relative to your terrain scale
        float hPosX = GetTerrainHeight(rHash1 + float2(step, 0.0), maxBladePosition);
        float hNegX = GetTerrainHeight(rHash1 - float2(step, 0.0), maxBladePosition);
        float hPosZ = GetTerrainHeight(rHash1 + float2(0.0, step), maxBladePosition);
        float hNegZ = GetTerrainHeight(rHash1 - float2(0.0, step), maxBladePosition);

        float dX = (hPosX - hNegX) / (2.0 * step);
        float dZ = (hPosZ - hNegZ) / (2.0 * step);

        slopeAngle = degrees(atan(sqrt(dX * dX + dZ * dZ)));

        // Check sand/water level
        float avgPos = (hPosX + hNegX + hPosZ + hNegZ) / 4;

        elevation = avgPos;

        attempt++;
    } while (slopeAngle >= 4.7f && attempt < maxAttempts && elevation < -0.63f);

    // Values are determined through testing
    if (slopeAngle >= 4.7f || elevation < -0.63) {
        return;
    }

    float3 bladePosition = float3(rHash1.x + chunkPosition.x, 0, rHash1.y + chunkPosition.y);

    GrassData[offsetId].worldPosition = bladePosition.xyz;

    float rHash2 = HashFloatInRange(offsetId ^ 0x6C8E9CF5u, 0, 16);

    GrassData[offsetId].rotation = rescale(uhash11(offsetId + 1), 0, 4) * 2 * 3.141592653589793f; // To radiance

    GrassData[offsetId].height = HashFloatInRange(offsetId + 500, 0.1, 1.1);

}