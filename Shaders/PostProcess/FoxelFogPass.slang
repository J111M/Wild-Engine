
cbuffer FroxelConstants : register(b0)
{
    float4x4 inverseProjection;
    float4x4 inverseView;
    float3 cameraPosition;
    float cameraNear;
    float cameraFar;
    uint3 froxelResolution;
    float3 Jitter;
}

RWTexture3D<float4> FogVolume : register(u0);

// Froxel coordinate to world position
float3 FroxelToWorldPos(uint3 froxelCoord)
{
    // Froxel center in UV space [0,1]
    float3 uvw = (float3(froxelCoord) + 0.5 + Jitter) / float3(froxelResolution);

    // UV to NDC
    float2 ndc = float2(uvw.x * 2.0 - 1.0, 1.0 - uvw.y * 2.0);

    // Unproject view space ray direction
    float4 viewRayH = mul(inverseProjection, float4(ndc, 0.0, 1.0));
    float3 viewRay = viewRayH.xyz / viewRayH.w;

    // Liniarize depth
    float linearDepth = cameraNear * pow(cameraFar / cameraNear, uvw.z);

    // Scale ray to reach this depth
    // Negative z because were using rh coordinates
    float3 viewPos = viewRay * (linearDepth / (-viewRay.z));

    // View to world space
    return mul(inverseView, float4(viewPos, 1.0)).xyz;
}

[shader("compute")]
[numthreads(8, 8, 4)]
void main(uint3 id: SV_DispatchThreadID)
{
    // If any of the thread id exceed the resolution return
    if (any(id >= froxelResolution)) return;

    float3 worldPos = FroxelToWorldPos(id);
}