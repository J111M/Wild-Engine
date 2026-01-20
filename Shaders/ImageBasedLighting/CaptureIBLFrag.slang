cbuffer Constants : register(b0)
{
    column_major float4x4 projView;
    uint skyView;

};

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

    float3 color = textures[skyView].Sample(linearSampler, uv.xy).rgb;
    color = color / (color + float3(1.0));
    color = pow(color, 1.0 / 2.2);

    return float4(color, 1.0f);
}