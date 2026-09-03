import DDGI.RayTracingUtils;
import DDGI.DDGIProbeData;
import DDGI.DDGITraceConstants;

struct RayPayload {
    float3 radiance;
    float hitDistance;
};

struct ShadowPayload {
    int occluded;
};

struct PointLight {
    float4 colorIntensity; // rgb = color, w = intensity
    float4 lightPosition;
};

#define MAX_POINT_LIGHTS 15

cbuffer PointLightData : register(b1) { PointLight pointLights[MAX_POINT_LIGHTS]; }

RaytracingAccelerationStructure tlas : register(t0);

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

    float3 emissiveFactor;
    float meshPadding;
};

StructuredBuffer<MeshInstanceInfo> meshTable : register(t1, space0);

StructuredBuffer<float4> probes : register(t2, space0);
StructuredBuffer<float4> probeIrradiance : register(t3, space0);

struct ProbeRayData {
    float3 radiance;
    float hitDistance;
};

RWStructuredBuffer<ProbeRayData> probeRayData : register(u0, space0);

ByteAddressBuffer meshBuffers[] : register(t0, space1);
Texture2D textures[] : register(t0, space2);
TextureCube cubeTextures[] : register(t0, space3);

SamplerState linearSampler : register(s0, space0);
SamplerState linearSamplerClamp : register(s1, space0);

static const uint VERTEX_STRIDE = 80;
static const uint VOFS_POSITION = 0;
static const uint VOFS_COLOR = 16;
static const uint VOFS_NORMAL = 32;
static const uint VOFS_UV = 48;
static const uint VOFS_TANGENT = 64;

static const float SHADOW_RAY_NORMAL_OFFSET = 0.02f;
static const float SHADOW_RAY_TMIN = 0.01f;
static const float BACKFACE_DISTANCE_SCALE = 0.2f;

bool TraceShadowRay(float3 origin, float3 direction, float maxDistance) {
    ShadowPayload shadowPayload;
    shadowPayload.occluded = 1;

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = SHADOW_RAY_TMIN;
    ray.TMax = maxDistance;

    TraceRay(tlas,
             RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
             0xFF, 0, 0, 1, ray, shadowPayload);

    return shadowPayload.occluded == 0;
}

[shader("raygeneration")]
void RayGen() {
    DDGIProbeData probeData = GetDDGIProbeData(probeDataView);

    uint rayIndex = DispatchRaysIndex().x;
    uint probeIndex = DispatchRaysIndex().y;

    float3 probePosition = probes[probeIndex].xyz;
    float3 randomDirection = GetRayDirection(rayIndex, probeData.raysPerProbe, probeData.randomRotation);

    RayDesc ray;
    ray.Origin = probePosition;
    ray.Direction = randomDirection;
    ray.TMin = 0.01f;
    ray.TMax = probeData.maxRayDistance;

    RayPayload payload;
    payload.radiance = float3(0, 0, 0);
    payload.hitDistance = probeData.maxRayDistance;

    TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    uint rayDataIndex = probeIndex * probeData.GetMaxRaysPerProbe() + rayIndex;

    float3 radiance = payload.radiance;
    bool invalidRadiance = any(isnan(radiance)) || any(isinf(radiance));
    radiance = invalidRadiance ? float3(0, 0, 0) : max(radiance, float3(0, 0, 0));

    float hitDistance = payload.hitDistance;
    bool invalidDistance = isnan(hitDistance) || isinf(hitDistance);
    hitDistance = invalidDistance ? probeData.maxRayDistance : hitDistance;

    probeRayData[rayDataIndex].radiance = radiance;
    probeRayData[rayDataIndex].hitDistance = hitDistance;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, BuiltInTriangleIntersectionAttributes attribs) {
    DDGIProbeData probeData = GetDDGIProbeData(probeDataView);

    MeshInstanceInfo mesh = meshTable[InstanceID()];

    ByteAddressBuffer vb = meshBuffers[mesh.vbHandle];
    ByteAddressBuffer ib = meshBuffers[mesh.ibHandle];

    uint3 tri = ib.Load3(PrimitiveIndex() * 12);

    uint v0 = tri.x * VERTEX_STRIDE;
    uint v1 = tri.y * VERTEX_STRIDE;
    uint v2 = tri.z * VERTEX_STRIDE;

    float3 b = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y,
                      attribs.barycentrics.x,
                      attribs.barycentrics.y);

    float3 n0 = asfloat(vb.Load3(v0 + VOFS_NORMAL));
    float3 n1 = asfloat(vb.Load3(v1 + VOFS_NORMAL));
    float3 n2 = asfloat(vb.Load3(v2 + VOFS_NORMAL));

    float2 uv0 = asfloat(vb.Load2(v0 + VOFS_UV));
    float2 uv1 = asfloat(vb.Load2(v1 + VOFS_UV));
    float2 uv2 = asfloat(vb.Load2(v2 + VOFS_UV));

    float3 normalOS = normalize(n0 * b.x + n1 * b.y + n2 * b.z);
    float2 uv = uv0 * b.x + uv1 * b.y + uv2 * b.z;

    float3 normalWS = normalize(mul((float3x3)WorldToObject4x3(), normalOS));

    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    if (dot(normalWS, WorldRayDirection()) > 0.0) {
        payload.radiance = float3(0, 0, 0);
        payload.hitDistance = -max(RayTCurrent() * BACKFACE_DISTANCE_SCALE, 0.01f);
        return;
    }

    float3 albedo = (mesh.albedoView != 0)
        ? pow(textures[NonUniformResourceIndex(mesh.albedoView)].SampleLevel(linearSampler, uv, 0).rgb, 2.2f)
        : float3(1, 1, 1);

    float3 emissive = (mesh.emissiveView != 0)
        ? pow(textures[NonUniformResourceIndex(mesh.emissiveView)].SampleLevel(linearSampler, uv, 0).rgb, 2.2f)
        : float3(1, 1, 1);
    emissive *= mesh.emissiveFactor * mesh.emissiveStrength * emissiveIntensity;

    float3 diffuse = DiffuseBRDF(albedo);
    float3 shadowOrigin = hitPos + normalWS * SHADOW_RAY_NORMAL_OFFSET;

    float3 radiance = emissive;

    {
        float3 L = -normalize(lightDirection.xyz);
        float NdotL = saturate(dot(normalWS, L));

        if (NdotL > 0.0 && TraceShadowRay(shadowOrigin, L, 1e27f)) {
            radiance += diffuse * NdotL * lightColorIntensity.rgb * lightColorIntensity.a;
        }
    }

    uint lightCount = min(numPointLights, MAX_POINT_LIGHTS);
    for (uint i = 0; i < lightCount; i++) {
        float3 toLight = pointLights[i].lightPosition.xyz - shadowOrigin;
        float lightDistance = length(toLight);
        if (lightDistance <= 1e-4f) continue;

        float3 L = toLight / lightDistance;

        float NdotL = saturate(dot(normalWS, L));
        if (NdotL <= 0.0) continue;

        if (!TraceShadowRay(shadowOrigin, L, lightDistance)) continue;

        float attenuation = pointLights[i].colorIntensity.w / (lightDistance * lightDistance);
        radiance += diffuse * NdotL * pointLights[i].colorIntensity.rgb * attenuation;
    }

    radiance += DiffuseBRDF(min(albedo, float3(0.9f))) *
                SampleDDGIIrradiance(probeData, linearSamplerClamp, hitPos, normalWS, WorldRayDirection());

    payload.radiance = radiance;
    payload.hitDistance = RayTCurrent();
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    float3 dir = WorldRayDirection();
    payload.radiance = (environmentView != 0) ? cubeTextures[NonUniformResourceIndex(environmentView)].SampleLevel(linearSampler, dir, 0).rgb : float3(0, 0, 0);
    payload.hitDistance = GetDDGIProbeData(probeDataView).maxRayDistance;
}

[shader("miss")]
void ShadowMiss(inout ShadowPayload payload) {
    payload.occluded = 0;
}
