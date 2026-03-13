cbuffer Constants : register(b0)
{
    uint skyView;
    uint skyCubeView;
    uint mipLevel;
    uint pad;

    float4 lightDirectionIntensity;
};

cbuffer matrices : register(b1)
{
    column_major float4x4 view;
    column_major float4x4 projection;
}

texture2D textures[4096] : register(t0, space0);
TextureCube textureCube[4096] : register(t0, space1);
SamplerState linearSampler : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 UVW : TEXCOORD0;
};

static const float PI = 3.14159265358979323846f;

float2 SampleSphericalMap(float3 dir)
{
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

    float3 color;
    if (skyView > 0)
        color = textures[skyView].Sample(linearSampler, uv).rgb;

    color = color / (color + float3(1.0));
    color = pow(color, 1.0 / 2.2);

    if (skyCubeView > 0)
        color = textureCube[skyCubeView].SampleLevel(linearSampler, dir, mipLevel).rgb;

    // Sun
    float3 sunDir = normalize(lightDirectionIntensity.xyz);
    float sun = dot(dir, sunDir);
    float sunDisk = smoothstep(0.9995f, 1.0f, sun);
    float sunGlow = pow(max(sun, 0.0f), 64.0f) * 0.3f;
    color += float3(1.0f, 0.9f, 0.6f) * sunDisk * lightDirectionIntensity.w;
    color += float3(1.0f, 0.7f, 0.3f) * sunGlow * lightDirectionIntensity.w;

    return float4(color, 1.0f);
}
