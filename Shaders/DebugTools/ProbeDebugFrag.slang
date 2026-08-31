import DDGI.RayTracingUtils;

// Mirrors Wild::ProbeDebugRootConstants. ProbeDebugVert.slang declares only the leading projView and
// probeScale, so nothing here may be reordered ahead of those two.
cbuffer RootConstant : register(b0)
{
    column_major float4x4 projView;

    float probeScale;
    uint irradianceView;
    float irradianceExposure;
    float rootPadding;

    int4 probeCounts; // xyz = probe grid counts, w unused
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    nointerpolation uint instanceId : INSTANCE_ID;
};

SamplerState linearClampSampler : register(s0);

[shader("fragment")]
float4 main(VSOutput input) : SV_Target0
{
    float3 normal = normalize(input.normal);

    uint3 probeGridCoord = GetProbeGridCoord(input.instanceId, probeCounts.xyz);
    float3 uv = GetProbeAtlasUV(probeGridCoord, probeCounts.xyz, normal, PROBE_IRRADIANCE_TEXELS);

    // Raw heap slot, the atlas is a Texture2DArray which cannot come through a bindless Texture2D range
    Texture2DArray<float4> irradianceAtlas = DescriptorHandle<Texture2DArray<float4>>(irradianceView);
    float3 irradiance = irradianceAtlas.SampleLevel(linearClampSampler, uv, 0).rgb;

    irradiance = pow(max(irradiance, 0.0f), PROBE_IRRADIANCE_GAMMA);

    irradiance *= irradianceExposure;
    float3 color = irradiance / (1.0f + irradiance);

    color = pow(color, 1.0f / 2.2f);

    return float4(max(color, 0.02f), 1.0f);
}
