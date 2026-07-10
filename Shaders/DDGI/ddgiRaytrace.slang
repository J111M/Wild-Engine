import DDGI.RaytracingUtils;

#define PROBE_DISTANCE_TEXELS 14
#define PROBE_IRRADIANCE_TEXELS 6

struct RayPayload {
    float3 radiance;
    float hitDistance;
};

struct ShadowPayload {
    int occluded;
};

cbuffer DDGIConstants : register(b0) {
    column_major float4x4 inverseView;

    float4 randomRotation; // quaternion (xyz axis*sin(half), w = cos(half))

    float4 lightDirection;      // xyz = direction the light travels, w unused
    float4 lightColorIntensity; // rgb = color, a = intensity

    float4 probeOrigin;  // xyz = grid origin, w unused
    float4 probeSpacing; // xyz = grid spacing, w unused
    int4 probeCounts;    // xyz = probe grid counts, w = maxRaysPerProbe (buffer stride)

    uint raysPerProbe;
    float maxRayDistance;
    float intensity;
    uint environmentView;

    uint numPointLights;
    uint IrradianceView;
    uint DistanceView;
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
};

StructuredBuffer<MeshInstanceInfo> meshTable : register(t1, space0);

StructuredBuffer<float4> probes : register(t2, space0);
StructuredBuffer<float4> probeIrradiance : register(t3, space0);

struct ProbeRayData {
    float3 radiance;
    float hitDistance;
};

RWStructuredBuffer<ProbeRayData> probeRayData : register(u0, space0);

// Use srv read
// Format: R11G11B10F
//RWTexture2D<float3> probeIrradianceTex : register(u1, space0);
// Format RG16F
//RWTexture2D<float2> probeDistanceTex : register(u2, space0);

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

static const float PI = 3.14159265359;

static const float INV_GOLDEN_RATIO = 0.6180339887498949;

// Evenly distributes `n` directions over the sphere.
float3 SphericalFibonacci(uint i, uint n) {
    float phi = 2.0 * PI * frac(float(i) * INV_GOLDEN_RATIO);
    float cosTheta = 1.0 - (2.0 * float(i) + 1.0) / float(n);
    float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));
    return float3(cos(phi) * sinTheta, cosTheta, sin(phi) * sinTheta);
}

float3 RotateByQuaternion(float3 v, float4 q) {
    float3 qv = q.xyz;
    float3 t = 2.0 * cross(qv, v);
    return v + q.w * t + cross(qv, t);
}

[shader("raygeneration")]
void RayGen() {
    uint rayIndex = DispatchRaysIndex().x;
    uint probeIndex = DispatchRaysIndex().y;

    float3 probePosition = probes[probeIndex].xyz;
    float3 randomDirection = normalize(RotateByQuaternion(SphericalFibonacci(rayIndex, raysPerProbe), randomRotation));

    RayDesc ray;
    ray.Origin = probePosition;
    ray.Direction = randomDirection;
    ray.TMin = 0.01f;
    ray.TMax = maxRayDistance;

    RayPayload payload;
    payload.radiance = float3(0, 0, 0);
    payload.hitDistance = maxRayDistance;

    TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    uint rayDataIndex = probeIndex * uint(probeCounts.w) + rayIndex;

    // TODO merge into 1 float4 to reduce shader lookups and storing
    probeRayData[rayDataIndex].radiance = payload.radiance;
    probeRayData[rayDataIndex].hitDistance = payload.hitDistance;
}

float Min(float3 v)
{
    return min(v.x, min(v.y, v.z));
}

float3 GetProbeLocationFromGridCoord(uint3 gridCoord)
{
    return probeOrigin.xyz + probeSpacing.xyz * gridCoord;
}

uint2 GetProbeTexel(uint3 probeIndex3D, uint numProbeInteriorTexels)
{
    uint numProbeTexels = 1 + numProbeInteriorTexels + 1;
    return uint2(probeIndex3D.x + probeIndex3D.y * probeCounts.x, probeIndex3D.z) * numProbeTexels + 1;
}

float2 GetProbeUV(uint3 probeIndex3D, float3 direction, uint numProbeInteriorTexels)
{
    uint numProbeTexels = 1 + numProbeInteriorTexels + 1;
    uint textureWidth = numProbeTexels * probeCounts.x * probeCounts.y;
    uint textureHeight = numProbeTexels * probeCounts.z;

    float2 pixel = GetProbeTexel(probeIndex3D, numProbeInteriorTexels);
    pixel += (EncodeNormalOctahedron(normalize(direction)) * 0.5f + 0.5f) * numProbeInteriorTexels;
    return pixel / float2(textureWidth, textureHeight);
}

float3 SampleDDGIIrradiance(float3 P, float3 N, float3 Wo) {
    float3 direction = N;
    float3 position = P;
    float volumeWeight = 1.0f;

    float3 relativeCoordinates = (position - probeOrigin.xyz) / probeSpacing.xyz;
    for (uint i = 0; i < 3; ++i)
    {
        volumeWeight *= lerp(0, 1, saturate(relativeCoordinates[i]));
        if (relativeCoordinates[i] > probeCounts[i] - 2)
        {
            float x = saturate(relativeCoordinates[i] - probeCounts[i] + 2);
            volumeWeight *= lerp(1, 0, x);
        }
    }

    // If weight = 0 than the probes didn't capture any data
    if (volumeWeight <= 0.0f) return 0.0f;

    const float normalBiasMultiplier = 0.2f;
    const float viewBiasMultiplier = 0.8f;
    const float axialDistanceMultiplier = 0.75f;
    position += (direction * normalBiasMultiplier + -Wo * viewBiasMultiplier) * axialDistanceMultiplier * Min(probeSpacing.xyz) * 0.2f;

    uint3 baseProbeCoordinates = floor(relativeCoordinates);
    float3 baseProbePosition = GetProbeLocationFromGridCoord(baseProbeCoordinates);
    float3 alpha = saturate((position - baseProbePosition) / probeSpacing.xyz);

    float3 sumIrradiance = 0;
    float sumWeight = 0;

    for (uint i = 0; i < 8; ++i)
    {
        uint3 indexOffset = uint3(i, i >> 1u, i >> 2u) & 1u;

        uint3 probeCoordinates = clamp(baseProbeCoordinates + indexOffset, 0, probeCounts.xyz - 1);
        float3 probePosition = GetProbeLocationFromGridCoord(probeCoordinates.xyz);

        float3 relativeProbePosition = position - probePosition;
        float3 probeDirection = -normalize(relativeProbePosition);

        float3 trilinear = max(0.001f, lerp(1.0f - alpha, alpha, indexOffset));
        float trilinearWeight = (trilinear.x * trilinear.y * trilinear.z);

        float weight = 1;

        weight *= saturate(dot(probeDirection, direction));

        float2 distanceUV = GetProbeUV(probeCoordinates, -probeDirection, PROBE_DISTANCE_TEXELS);
        float probeDistance = length(relativeProbePosition);

        float2 moments = textures[DistanceView].SampleLevel(linearSamplerClamp, distanceUV, 0).xy;
        float variance = abs(Pow2(moments.x) - moments.y);
        float chebyshev = 1.0f;
        if (probeDistance > moments.x)
        {
            float mD = moments.x - probeDistance;
            chebyshev = variance / (variance + Pow2(mD));
            chebyshev = max(Pow3(chebyshev), 0.0);
        }
        weight *= max(chebyshev, 0.05f);
        weight = max(0.000001f, weight);

        const float crushThreshold = 0.2f;
        if (weight < crushThreshold)
		{
			weight *= weight * weight * (1.0f / Pow2(crushThreshold));
		}
        weight *= trilinearWeight;

        float2 uv = GetProbeUV(probeCoordinates, direction, PROBE_IRRADIANCE_TEXELS);
        float3 irradiance = textures[IrradianceView].SampleLevel(linearSamplerClamp, uv, 0).rgb;
        irradiance = pow(irradiance, 2.5f);

        sumIrradiance += irradiance * weight;
        sumWeight += weight;
    }

    if (sumWeight == 0) return 0.0f;

    sumIrradiance *= (1.0f / sumWeight);
    sumIrradiance *= sumIrradiance;
    sumIrradiance *= PI;
    return sumIrradiance * volumeWeight;
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

    // World hit position TODO possibly change to use reconstructed worldspace from the buffer to avoid self intersection
    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    // Backface hits are discarded by the blend pass (negative distance) - don't waste a shadow ray on them.
    if (dot(normalWS, WorldRayDirection()) > 0.0) {
        payload.radiance = float3(0, 0, 0);
        payload.hitDistance = -max(RayTCurrent(), 0.01);
        return;
    }

    float3 albedo = (mesh.albedoView != 0)
        ? textures[NonUniformResourceIndex(mesh.albedoView)].SampleLevel(linearSampler, uv, 0).rgb
    : float3(1, 1, 1);

    float3 emissive = (mesh.emissiveView != 0)
        ? textures[NonUniformResourceIndex(mesh.emissiveView)].SampleLevel(linearSampler, uv, 0).rgb
    : float3(0, 0, 0);
    emissive *= mesh.emissiveStrength;

    float3 radiance = float3(0.0f);

    // Directional light TODO make the approach cleaner
    {

        float3 lightDirWS = normalize(mul(lightDirection.xyz, (float3x3)inverseView));
        float3 L = normalize(-lightDirWS);

        float NdotL = saturate(dot(normalWS, L));
        if (NdotL > 0.0) {
            ShadowPayload shadowPayload;
            shadowPayload.occluded = 1;

            RayDesc ray;
            ray.Origin = hitPos;
            ray.Direction = L;
            ray.TMin = 1e-1f;
            ray.TMax = 1e27f;

            TraceRay(tlas,
                     RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                     0xFF, 0, 0, 1, ray, shadowPayload);

            float visibility = (shadowPayload.occluded != 0) ? 0.0 : 1.0;

            if (visibility > 0) {
                float3 diffuse = saturate(dot(L, normalWS * DiffuseBRDF(albedo)));
                radiance += diffuse * (lightColorIntensity.rgb * lightColorIntensity.a);
            }
        }
    }

    // Trace light visibility and data for light types
    for (uint i = 0; i < numPointLights; i++) {
        bool visibility = false;

        float3 toLight = pointLights[i].lightPosition.xyz - hitPos;
        float lightDistance = length(toLight);
        float3 lightDir = toLight / lightDistance;

        // Transform the light direction and position to world space
        lightDir = mul(lightDir, (float3x3)inverseView);
        float3 lightPosition = mul(float4(pointLights[i].lightPosition.xyz, 1.0f), inverseView).xyz;

        // Cull the light if the angle to the light is smaller than 0
        float NdotL = saturate(dot(normalWS, lightDir));
        if (NdotL <= 0.0) continue;

        // World space calculation
        float3 direction = lightPosition - hitPos;
        float maxT = length(direction);

        ShadowPayload shadowPayload;
        shadowPayload.occluded = 1;

        RayDesc ray;
        ray.Origin = hitPos;
        ray.Direction = normalize(direction);
        ray.TMin = 1e-1f;
        ray.TMax = maxT;

        TraceRay(tlas,
                 RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
                 0xFF, 0, 0, 1, ray, shadowPayload);

        visibility = (shadowPayload.occluded != 0) ? 0.0 : 1.0;
        if (!visibility) continue;

        float3 L = mul(lightDir, (float3x3)inverseView);
        L = normalize(-L);

        float3 diffuse = saturate(dot(L, normalWS * DiffuseBRDF(albedo)));
        radiance += diffuse * (pointLights[i].colorIntensity.rgb * lightColorIntensity.a);

    }

    radiance += emissive;
    radiance += DiffuseBRDF(min(albedo, float3(0.9f)) * SampleDDGIIrradiance(hitPos, normalWS, WorldRayDirection())); // Sample ddgi 
   
    payload.radiance = radiance;
    payload.hitDistance = min(RayTCurrent(), payload.hitDistance);
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    float3 dir = WorldRayDirection();
    dir.y = -dir.y;
    payload.radiance = (environmentView != 0) ? cubeTextures[NonUniformResourceIndex(environmentView)].SampleLevel(linearSampler, dir, 0).rgb : float3(0, 0, 0);
    payload.hitDistance = maxRayDistance;
}

[shader("miss")]
void ShadowMiss(inout ShadowPayload payload) {
    payload.occluded = 0;
}
