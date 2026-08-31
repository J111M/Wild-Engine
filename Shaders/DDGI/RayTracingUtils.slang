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