struct RayPayload {
    float3 color;
    float3 hitPos;
    float3 normal;
    float2 uv;
};

cbuffer CameraConstants : register(b0) {
    column_major float4x4 viewInverse;
    column_major float4x4 projInverse;

    uint environmentView;
};

RaytracingAccelerationStructure tlas : register(t0);

RWTexture2D<float4> output : register(u0);

struct MeshInstanceInfo {
    uint vbHandle;
    uint ibHandle;

    uint albedoView;
    uint normalView;
    uint roughnessMetallicView;
    uint emissiveView;

    uint ambientOcclussionView;
    float roughness;
    float metallic;
    float emissiveStrength;
};

StructuredBuffer<MeshInstanceInfo> meshTable : register(t1, space0);

ByteAddressBuffer meshBuffers[] : register(t0, space1);
Texture2D textures[] : register(t0, space2);
TextureCube cubeTextures[] : register(t0, space3);

SamplerState linearSampler : register(s0, space0);

static const uint VERTEX_STRIDE = 80;
static const uint VOFS_POSITION = 0;
static const uint VOFS_COLOR = 16;
static const uint VOFS_NORMAL = 32;
static const uint VOFS_UV = 48;
static const uint VOFS_TANGENT = 64;

[shader("raygeneration")]
void RayGen() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    float2 uv = (float2(launchIndex) + 0.5) / float2(launchDim);
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

    float4 target = mul(projInverse, float4(ndc, 1.0, 1.0));
    float3 dirVS = target.xyz / target.w;

    float3 origin = mul(viewInverse, float4(0, 0, 0, 1)).xyz;
    float3 dir = normalize(mul(viewInverse, float4(dirVS, 0.0)).xyz);

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    RayPayload payload;
    payload.color = float3(0, 0, 0);

    TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    output[launchIndex] = float4(payload.color, 1.0);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, BuiltInTriangleIntersectionAttributes attribs) {
    MeshInstanceInfo mesh = meshTable[InstanceID()];

    ByteAddressBuffer vb = meshBuffers[mesh.vbHandle];
    ByteAddressBuffer ib = meshBuffers[mesh.ibHandle];

    uint3 tri = ib.Load3(PrimitiveIndex() * 12);

    uint v0 = tri.x * VERTEX_STRIDE;
    uint v1 = tri.y * VERTEX_STRIDE;
    uint v2 = tri.z * VERTEX_STRIDE;

    // Barycentrics
    float3 b = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y,
                      attribs.barycentrics.x,
                      attribs.barycentrics.y);

    float3 n0 = asfloat(vb.Load3(v0 + VOFS_NORMAL));
    float3 n1 = asfloat(vb.Load3(v1 + VOFS_NORMAL));
    float3 n2 = asfloat(vb.Load3(v2 + VOFS_NORMAL));

    float2 uv0 = asfloat(vb.Load2(v0 + VOFS_UV));
    float2 uv1 = asfloat(vb.Load2(v1 + VOFS_UV));
    float2 uv2 = asfloat(vb.Load2(v2 + VOFS_UV));

    float3 normalOS = n0 * b.x + n1 * b.y + n2 * b.z;
    float2 uv = uv0 * b.x + uv1 * b.y + uv2 * b.z;

    float3 normalWS = normalize(mul(normalOS, (float3x3)WorldToObject4x3()));

    float3 albedo = (mesh.albedoView != 0)
        ? textures[NonUniformResourceIndex(mesh.albedoView)].SampleLevel(linearSampler, uv, 0).rgb
    : float3(1, 1, 1);

    float2 rm = (mesh.roughnessMetallicView != 0)
        ? textures[NonUniformResourceIndex(mesh.roughnessMetallicView)].SampleLevel(linearSampler, uv, 0).gb
                                                  : float2(mesh.roughness, mesh.metallic);

    float3 emissive = (mesh.emissiveView != 0)
        ? textures[NonUniformResourceIndex(mesh.emissiveView)].SampleLevel(linearSampler, uv, 0).rgb
    : float3(0, 0, 0);

    float ao = (mesh.ambientOcclussionView != 0)
        ? textures[NonUniformResourceIndex(mesh.ambientOcclussionView)].SampleLevel(linearSampler, uv, 0).r
    : 1.0;

    float roughness = rm.x;
    float metallic = rm.y;
    emissive *= mesh.emissiveStrength;

    // Hit position in world space
    payload.hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    
    payload.uv = attribs.barycentrics;

    payload.color = albedo;
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    float3 dir = WorldRayDirection();
    dir.y = -dir.y;
    payload.color = (environmentView != 0) ? cubeTextures[NonUniformResourceIndex(environmentView)].SampleLevel(linearSampler, dir, 0).rgb : float3(0, 0, 0);
}