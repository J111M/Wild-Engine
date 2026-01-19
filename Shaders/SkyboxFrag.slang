cbuffer Constants : register(b0)
{
    uint skyView;

};

cbuffer matrices : register(b1)
{
    column_major float4x4 view;
    column_major float4x4 projection;
}

Texture2D textures[4096] : register(t0, space0);
SamplerState linearSampler : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 UVW : TEXCOORD0;
};

static const float PI = 3.14159265358979323846f;

float2 SampleSphericalMap(float3 dir) {
    dir = normalize(dir);

    float u = atan2(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float v = asin(dir.y) / PI + 0.5;

    return float2(u, 1 - v);
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target0
{
    float3 dir = normalize(input.UVW);

    // Calculate uv for equirectangular map
    float2 uv = SampleSphericalMap(dir);


    return textures[skyView].Sample(linearSampler, uv.xy);
}