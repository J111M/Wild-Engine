import DDGI.DDGIProbeData;

static const float MIN_WEIGHT = 0.0001f;

float2 SignNotZero(float2 v) {
    return float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

float2 EncodeNormalOctahedron(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * SignNotZero(n.xy);
    return n.xy;
}

float2 OctEncode(float3 d) {
    float2 p = d.xy * (1.0 / (abs(d.x) + abs(d.y) + abs(d.z)));
    return (d.z <= 0.0) ? ((1.0 - abs(p.yx)) * SignNotZero(p)) : p;
}

float3 OctDecode(float2 f) {
    float3 d = float3(f.xy, 1.0 - abs(f.x) - abs(f.y));
    if (d.z < 0.0) d.xy = (1.0 - abs(d.yx)) * SignNotZero(d.xy);
    return normalize(d);
}

float3 TexelToDir(int2 interiorXY, int N) {
    float2 oct = (float2(interiorXY) + 0.5) / float(N);
    return OctDecode(oct * 2.0 - 1.0);
}

static const float M_PI_INV = 0.318309886f;

float3 DiffuseBRDF(float3 albedo) {
    return albedo * M_PI_INV;
}

static const float PI = 3.14159265359;
static const float INV_GOLDEN_RATIO = 0.6180339887498949;

static const int PROBE_IRRADIANCE_TEXELS = 6;
static const int PROBE_DISTANCE_TEXELS = 14;

uint3 GetProbeTexel(uint3 probeGridCoord, uint numProbeInteriorTexels)
{
    uint numProbeTexels = numProbeInteriorTexels + 2;

    // Scale the in plane axes only, appending the slice afterwards, a uint3(x, z, y) * n + 1 would scale and
    // offset the plane index too.
    return uint3(uint2(probeGridCoord.x, probeGridCoord.z) * numProbeTexels + 1, probeGridCoord.y);
}

float3 GetProbeAtlasUV(uint3 probeGridCoord, int3 probeCounts, float3 direction, uint numProbeInteriorTexels)
{
    uint numProbeTexels = numProbeInteriorTexels + 2;
    float2 textureSize = float2(numProbeTexels * probeCounts.x, numProbeTexels * probeCounts.z);

    uint3 coords = GetProbeTexel(probeGridCoord, numProbeInteriorTexels);

    // coords.xy is the probe's first interior texel, the octant coordinate then spans the interior block,
    // so the sample never leaves the probe's own texels and its border ring.
    float2 pixel = float2(coords.xy);
    pixel += (EncodeNormalOctahedron(normalize(direction)) * 0.5f + 0.5f) * numProbeInteriorTexels;

    return float3(pixel / textureSize, float(coords.z));
}

uint3 GetProbeGridCoord(uint probeIndex, int3 probeCounts)
{
    uint countX = uint(probeCounts.x);
    uint countY = uint(probeCounts.y);

    return uint3(probeIndex % countX, (probeIndex / countX) % countY, probeIndex / (countX * countY));
}

static const float PROBE_IRRADIANCE_GAMMA = 5.0f;

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

float3 GetRayDirection(uint rayIndex, uint raysPerProbe, float4 randomRotation) {
    return normalize(RotateByQuaternion(SphericalFibonacci(rayIndex, raysPerProbe), randomRotation));
}

float Pow2(float x) { return x * x; }
float Pow3(float x) { return x * x * x; }

float Min(float3 v)
{
    return min(v.x, min(v.y, v.z));
}

float3 GetProbeLocationFromGridCoord(DDGIProbeData probeData, uint3 gridCoord)
{
    return probeData.probeOrigin.xyz + probeData.probeSpacing.xyz * gridCoord;
}

// The atlas is one 2D slice per probe grid Y plane, so only X and Z scale its 2D footprint. Returns a
// Texture2DArray sample coordinate, xy the UV within the slice and z the slice index.
float3 GetProbeUV(DDGIProbeData probeData, uint3 probeGridCoord, float3 direction, uint numProbeInteriorTexels)
{
    return GetProbeAtlasUV(probeGridCoord, probeData.probeCounts.xyz, direction, numProbeInteriorTexels);
}

float3 GetSurfaceBias(DDGIProbeData probeData, float3 N, float3 V)
{
    float minSpacing = Min(probeData.probeSpacing.xyz);
    float3 bias = (N * probeData.normalBias + V * probeData.viewBias) * minSpacing;
    float maxLength = 0.5f * minSpacing;
    float biasLength = length(bias);
    return (biasLength > maxLength) ? bias * (maxLength / biasLength) : bias;
}

float GetVolumeWeight(DDGIProbeData probeData, float3 relativeCoordinates)
{
    float volumeWeight = 1.0f;
    for (uint axis = 0; axis < 3; ++axis)
    {
        volumeWeight *= saturate(relativeCoordinates[axis]);
        volumeWeight *= saturate(float(probeData.probeCounts[axis]) - 1.0f - relativeCoordinates[axis]);
    }
    return volumeWeight;
}

float3 SampleDDGIIrradiance(DDGIProbeData probeData, SamplerState clampSampler, float3 P, float3 N, float3 Wo) {
    float3 relativeCoordinates = (P - probeData.probeOrigin.xyz) / probeData.probeSpacing.xyz;

    float volumeWeight = GetVolumeWeight(probeData, relativeCoordinates);
    if (volumeWeight <= 0.0f) return 0.0f;

    float3 biasedPosition = P + GetSurfaceBias(probeData, N, -Wo);
    float3 biasedRelativeCoordinates = (biasedPosition - probeData.probeOrigin.xyz) / probeData.probeSpacing.xyz;

    int3 baseProbeCoordinates = clamp(int3(floor(biasedRelativeCoordinates)), int3(0, 0, 0), probeData.probeCounts.xyz - 1);
    float3 baseProbePosition = GetProbeLocationFromGridCoord(probeData, uint3(baseProbeCoordinates));
    float3 alpha = saturate((biasedPosition - baseProbePosition) / probeData.probeSpacing.xyz);

    float3 sumIrradiance = 0;
    float sumWeight = 0;

    Texture2DArray<float4> irradianceAtlas = DescriptorHandle<Texture2DArray<float4>>(probeData.irradianceView);
    Texture2DArray<float2> distanceAtlas = DescriptorHandle<Texture2DArray<float2>>(probeData.distanceView);

    for (uint i = 0; i < 8; ++i)
    {
        int3 indexOffset = int3(i, i >> 1u, i >> 2u) & 1;

        uint3 probeCoordinates = uint3(clamp(baseProbeCoordinates + indexOffset, int3(0, 0, 0), probeData.probeCounts.xyz - 1));
        float3 probePosition = GetProbeLocationFromGridCoord(probeData, probeCoordinates);

        float3 directionToProbe = normalize(probePosition - P);
        float3 biasedToProbe = probePosition - biasedPosition;
        float biasedDistance = length(biasedToProbe);
        float3 biasedDirectionToProbe = biasedToProbe / max(biasedDistance, 1e-6f);

        float3 trilinear = max(0.001f, lerp(1.0f - alpha, alpha, float3(indexOffset)));
        float trilinearWeight = trilinear.x * trilinear.y * trilinear.z;

        float wrapShading = (dot(directionToProbe, N) + 1.0f) * 0.5f;
        float weight = Pow2(wrapShading) + 0.2f;

        float3 distanceUV = GetProbeUV(probeData, probeCoordinates, -biasedDirectionToProbe, PROBE_DISTANCE_TEXELS);
        float2 moments = 2.0f * distanceAtlas.SampleLevel(clampSampler, distanceUV, 0).xy;

        float chebyshev = 1.0f;
        if (biasedDistance > moments.x)
        {
            float variance = abs(Pow2(moments.x) - moments.y);
            float distanceDelta = biasedDistance - moments.x;
            chebyshev = variance / (variance + Pow2(distanceDelta));
            chebyshev = max(Pow3(chebyshev), 0.0f);
        }
        weight *= max(chebyshev, 0.05f);
        weight = max(0.000001f, weight);

        const float crushThreshold = 0.2f;
        if (weight < crushThreshold)
        {
            weight *= Pow2(weight) * (1.0f / Pow2(crushThreshold));
        }

        weight *= trilinearWeight;

        float3 uv = GetProbeUV(probeData, probeCoordinates, N, PROBE_IRRADIANCE_TEXELS);
        float3 irradiance = irradianceAtlas.SampleLevel(clampSampler, uv, 0).rgb;
        irradiance = pow(max(irradiance, 0.0f), PROBE_IRRADIANCE_GAMMA * 0.5f);

        sumIrradiance += irradiance * weight;
        sumWeight += weight;
    }

    if (sumWeight <= 0.0f) return 0.0f;

    sumIrradiance *= (1.0f / sumWeight);
    sumIrradiance *= sumIrradiance;
    sumIrradiance *= 2.0f * PI;
    return sumIrradiance * volumeWeight;
}
