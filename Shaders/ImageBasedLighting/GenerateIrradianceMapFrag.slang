cbuffer Constants : register(b0)
{
    column_major float4x4 projView;
    uint skyView;
};

TextureCube textures[4096] : register(t0, space0);
SamplerState linearSampler : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 UVW : TEXCOORD0;
};

static const float PI = 3.14159265358979323846f;

[shader("fragment")]
float4 main(VSOutput input) : SV_Target0
{
    float3 irradiance = float3(0.0);

    float3 normal = normalize(input.UVW);

    float3 up = float3(0.0, 1.0, 0.0);
    float3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));
    // Calculate uv for equirectangular map
    // float2 uv = SampleSphericalMap(dir);

    float sampleDelta = 0.025;
    float numSamples = 0.0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += textures[skyView].Sample(linearSampler, sampleVec).rgb * cos(theta) * sin(theta);
            numSamples++;
        }
    }

    irradiance = PI * irradiance * (1.0 / float(numSamples));

    return float4(irradiance, 1.0f);
}
