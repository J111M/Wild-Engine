float4 RestorePositionWS(float3 position, float4x4 inverseProjection, float4x4 inverseView)
{
    float4 positionVS = mul(inverseProjection, float4(position, 1.0));
    positionVS /= positionVS.w;

    return mul(inverseView, positionVS);
}

// Octahedral encoding
float3 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) { n.xy = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0); }
    return float3(n.xy * 0.5 + 0.5, 0.0);
}

// Octahederal decoding
float3 OctDecode(float2 enc)
{
    enc = enc * 2.0 - 1.0;
    float3 n = float3(enc, 1.0 - abs(enc.x) - abs(enc.y));
    if (n.z < 0.0) { n.xy = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0); }
    return normalize(n);
}

float remap(float value, float inMin, float inMax, float outMin, float outMax)
{
    return outMin + (value - inMin) * (outMax - outMin) / (inMax - inMin);
}