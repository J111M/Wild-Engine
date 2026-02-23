static const float PI = 3.14159265358979323846f;

// Normal distribution function (GGX)
float DistributionGGX(float NdotH, float a)
{
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;

    return nom / denom;
}

// Geometry (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float k)
{
    float nom = NdotV;
    float denom = NdotV * (1.0f - k) + k;
    return nom / denom;
}

float GeometrySmith(float NoV, float NoL, float k)
{
    float ggx1 = GeometrySchlickGGX(NoV, k);
    float ggx2 = GeometrySchlickGGX(NoL, k);
    return ggx1 * ggx2;
}

// Fresnel equations
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    // clamp to guard pow base
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 FresnelSchlickRough(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Outgoing radiance
float3 EvaluateDirectBRDF(
    float3 N,
    float3 V,
    float3 L,
    float3 albedo,
    float3 F0,
    float roughness,
    float metallic)
{
    float3 H = normalize(V + L);

    float ndotl = max(dot(N, L), 0.0f);
    float ndotv = max(dot(N, V), 0.0f);
    float hdotv = max(dot(H, V), 0.0f);
    float ndoth = max(dot(N, H), 0.0f);

    float NDF = DistributionGGX(ndoth, roughness);
    float G = GeometrySmith(ndotv, ndotl, roughness);
    float3 F = FresnelSchlick(hdotv, F0);

    float3 numerator = NDF * G * F;
    float denominator = max(4.0f * ndotv * ndotl, 0.0001f);
    float3 specular = max(numerator / denominator, float3(0.0f, 0.0f, 0.0f));

    float3 kS = F;
    float3 kD = 1.0f - kS;
    kD *= (1.0f - metallic);

    float3 lambertianDiffuse = (albedo / PI);

    return (kD * lambertianDiffuse + specular) * ndotl;
}