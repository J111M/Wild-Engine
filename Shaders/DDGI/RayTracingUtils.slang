

float2 SignNotZero(float2 v) {
    return float2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

float2 EncodeNormalOctahedron(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * lerp(-1.0, 1.0, n.xy >= 0.0);
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

float Pow2(float x) { return x * x; }
float Pow3(float x) { return x * x * x; }