// A module so the RayTracingUtils helpers don't clash with PbrFrag's includes
import DDGI.RayTracingUtils;
import DDGI.DDGIProbeData;

float3 DDGIGetSurfaceBias(DDGIProbeData probeData, float spacing, float3 N, float3 V)
{
    float3 bias = (N * probeData.normalBias + V * probeData.viewBias) * spacing;
    float maxLength = 0.5f * spacing;
    float biasLength = length(bias);
    return (biasLength > maxLength) ? bias * (maxLength / biasLength) : bias;
}

// Fades out over the last cascadeBlendCells cells so the next cascade takes over
float DDGIGetCascadeWeight(DDGIProbeData probeData, uint cascade, float3 P)
{
    DDGICascadeData cascadeData = probeData.cascades[cascade];

    float3 relative = P / cascadeData.spacing.x - float3(cascadeData.originCell.xyz);
    float3 lastProbe = float3(probeData.probeCounts.xyz - 1);
    float3 distanceToEdge = min(relative, lastProbe - relative);

    float blendCells = max(probeData.cascadeBlendCells, 1e-3f);
    float3 axisWeight = saturate(distanceToEdge / blendCells);
    return min(axisWeight.x, min(axisWeight.y, axisWeight.z));
}

float3 DDGISampleCascade(DDGIProbeData probeData, uint cascade, SamplerState clampSampler, float3 P, float3 N, float3 V)
{
    DDGICascadeData cascadeData = probeData.cascades[cascade];
    float spacing = cascadeData.spacing.x;
    int3 originCell = cascadeData.originCell.xyz;
    int3 lastProbe = probeData.probeCounts.xyz - 1;

    float3 biasedPosition = P + DDGIGetSurfaceBias(probeData, spacing, N, V);
    float3 biasedRelative = biasedPosition / spacing - float3(originCell);

    int3 baseLocal = clamp(int3(floor(biasedRelative)), int3(0, 0, 0), lastProbe);
    float3 alpha = saturate(biasedRelative - float3(baseLocal));

    Texture2DArray<float4> irradianceAtlas = DescriptorHandle<Texture2DArray<float4>>(probeData.irradianceView);
    Texture2DArray<float2> distanceAtlas = DescriptorHandle<Texture2DArray<float2>>(probeData.distanceView);

    float3 sumIrradiance = 0;
    float sumWeight = 0;

    for (uint i = 0; i < 8; ++i)
    {
        int3 indexOffset = int3(i, i >> 1u, i >> 2u) & 1;

        int3 worldCell = originCell + clamp(baseLocal + indexOffset, int3(0, 0, 0), lastProbe);
        uint3 atlasCoord = probeData.GetAtlasCoord(cascade, probeData.GetStorageFromWorldCell(worldCell));
        float3 probePosition = float3(worldCell) * spacing;

        float3 directionToProbe = normalize(probePosition - P);
        float3 biasedToProbe = probePosition - biasedPosition;
        float biasedDistance = length(biasedToProbe);
        float3 biasedDirectionToProbe = biasedToProbe / max(biasedDistance, 1e-6f);

        float3 trilinear = max(0.001f, lerp(1.0f - alpha, alpha, float3(indexOffset)));
        float trilinearWeight = trilinear.x * trilinear.y * trilinear.z;

        float wrapShading = (dot(directionToProbe, N) + 1.0f) * 0.5f;
        float weight = Pow2(wrapShading) + 0.2f;

        float3 distanceUV =
            GetProbeAtlasUV(atlasCoord, probeData.probeCounts.xyz, -biasedDirectionToProbe, PROBE_DISTANCE_TEXELS);
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
        if (weight < crushThreshold) { weight *= Pow2(weight) * (1.0f / Pow2(crushThreshold)); }

        weight *= trilinearWeight;

        float3 uv = GetProbeAtlasUV(atlasCoord, probeData.probeCounts.xyz, N, PROBE_IRRADIANCE_TEXELS);
        float3 irradiance = irradianceAtlas.SampleLevel(clampSampler, uv, 0).rgb;
        irradiance = pow(max(irradiance, 0.0f), PROBE_IRRADIANCE_GAMMA * 0.5f);

        sumIrradiance += irradiance * weight;
        sumWeight += weight;
    }

    if (sumWeight <= 0.0f) return 0.0f;

    sumIrradiance *= (1.0f / sumWeight);
    sumIrradiance *= sumIrradiance;
    sumIrradiance *= 2.0f * PI;
    return sumIrradiance;
}

// Alpha is how much the cascades cover
float4 SampleDDGIIrradiance(DDGIProbeData probeData, SamplerState clampSampler, float3 P, float3 N, float3 V)
{
    float3 result = 0;
    float remaining = 1.0f;

    uint cascadeCount = min(probeData.cascadeCount, DDGI_MAX_CASCADES);
    for (uint cascade = 0; cascade < cascadeCount; ++cascade)
    {
        float weight = DDGIGetCascadeWeight(probeData, cascade, P);
        if (weight <= 0.0f) continue;

        result += DDGISampleCascade(probeData, cascade, clampSampler, P, N, V) * (weight * remaining);
        remaining *= 1.0f - weight;

        if (remaining <= 0.001f) break;
    }

    return float4(result, 1.0f - remaining);
}
