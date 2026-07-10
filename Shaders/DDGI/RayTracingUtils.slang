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

float Pow2(float x) { return x * x; }
float Pow3(float x) { return x * x * x; }