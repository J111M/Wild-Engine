cbuffer Constants : register(b0)
{
    uint emissiveView;
    uint albedoView;
    uint normalView;
    uint depthView;
};

cbuffer matrices : register(b1)
{
    column_major matrix inverseView;
    column_major matrix inverseProjection;
}

cbuffer PbrData : register(b2)
{
    float4 cameraPosition;
    float4 lightDirection;
    uint debugViewMode;
 
}

cbuffer envData : register(b3)
{
    uint irradianceMapView;
    uint specularView;
    uint brdfView;
}

Texture2D textures[4096] : register(t0, space0);
TextureCube cubeTextures[4096] : register(t0, space1);

SamplerState pointSampler : register(s0);

float4 RestorePositionWS(float3 position)
{
    float4 positionVS = mul(inverseProjection, float4(position, 1.0));
    positionVS /= positionVS.w;

    return mul(inverseView, positionVS);
}

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

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

// Octahederal decoding
float3 OctDecode(float2 enc)
{
    enc = enc * 2.0 - 1.0;
    float3 n = float3(enc, 1.0 - abs(enc.x) - abs(enc.y));
    if (n.z < 0.0)
    {
        n.xy = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0);
    }
    return normalize(n);
}

[shader("fragment")]
float4 main(VSOutput input) : SV_TARGET0
{
    float4 albedo = float4(0);
    if (albedoView != 0)
        albedo = textures[albedoView].Sample(pointSampler, input.uv);

    float4 normal = float4(0);
    if (normalView != 0)
    {
        normal = textures[normalView].Sample(pointSampler, input.uv);
        // normal.y = -normal.y; // flip y
    }

    float4 emissive = float4(0);
    if (emissiveView != 0)
        emissive = textures[emissiveView].Sample(pointSampler, input.uv);

    // srgb to linear
    albedo.rgb = pow(albedo.rgb, float3(2.2f));
    emissive.rgb = pow(emissive.rgb, float3(2.2f));

    float roughness = albedo.a;
    float metallic = normal.a;
    float ao = emissive.a;
    if (ao > 1.0f)
        ao = 1.0f;

    // SSS and translucency
    float thickness = normal.b;

    // Reconstruct the position from the depth
    float3 positionNDC = float3(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0, textures[depthView].Sample(pointSampler, input.uv).x);
    // positionNDC.xy = (input.uv * 2) - 1;
    // positionNDC.y *= -1.0f;
    // positionNDC.z = ;

    float3 positionWS = RestorePositionWS(positionNDC).xyz;

    float3 color = float3(0.0f, 0.0f, 0.0f);

    // shading setup
    float3 N = OctDecode(normal.xy);
    float3 V = normalize(cameraPosition.xyz - positionWS);
    float3 R = reflect(-V, N);

    float3 L = normalize(lightDirection.xyz);
    float3 H = normalize(V + L);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo.rgb, metallic);

    float ndotl = max(dot(N, L), 0.0f);
    float ndotv = max(dot(N, V), 0.0f);
    float hdotv = max(dot(H, V), 0.0f);
    float ndoth = max(dot(N, H), 0.0f);

    float NDF = DistributionGGX(ndoth, roughness);
    float G = GeometrySmith(ndotv, ndotl, roughness);
    float3 F = FresnelSchlick(ndotv, F0);

    float3 numerator = NDF * G * F;
    float denominator = max(4.0f * ndotv * ndotl, 0.0001f);
    float3 specular = max(numerator / denominator, float3(0.0f, 0.0f, 0.0f));

    float3 kS = F;
    float3 kD = 1.0f - kS;
    kD *= (1.0f - metallic);

    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    float3 lambertianDiffuse = (albedo.rgb / PI);

    // IBL contribution
    Lo = (kD * lambertianDiffuse + specular) * ndotl; // add light color

    float3 irradiance = float3(0.0f);
    if (irradianceMapView > 0)
        irradiance = cubeTextures[irradianceMapView].Sample(pointSampler, N).rgb;
    else
    {
        irradiance = float3(0.2f, 0.2f, 0.2f);
    }

    float3 iblF = FresnelSchlickRough(ndotv, F0, roughness);
    float3 iblkS = iblF;
    float3 iblkD = 1.0 - iblkS;
    iblkD *= 1.0f - metallic;

    // Precomputed specular reflection
    const float maxLod = 4.0f;
    float3 prefilteredReflection = float3(0.0f);
    if (specularView > 0)
        prefilteredReflection = cubeTextures[specularView].SampleLevel(pointSampler, R, roughness * maxLod).rgb;

    float2 envBrdf = float2(1.0f, 0.0f);
    if (brdfView > 0)
        envBrdf = textures[brdfView].Sample(pointSampler, float2(ndotv, roughness)).rg;

    // envBrdf = max(envBrdf, 0.0f);

    float3 specularIBL = prefilteredReflection * iblkS; //(iblF * envBrdf.x + envBrdf.y); // Temporary disabled because the brdf is not correct

    float3 diffuse = albedo.rgb * irradiance;

    // Translucency
    float wrap = 0.5;
    float backlit = saturate((dot(V, -L) + wrap) / (1.0 + wrap));
    float falloff = 1.3;
    float translucencyMask = (1 - thickness) * falloff; // invert thickness to 0 being most thin and 1 having no effect;

    float3 scatterColor = float3(0.0f);
    if (thickness > 0)
        scatterColor = float3(1.1, 1.2, 0.9) * backlit * albedo.rgb * translucencyMask;

    float3 ambient = (iblkD * diffuse + specularIBL + scatterColor) * ao;

    color = Lo + ambient + emissive.rgb;

    switch (debugViewMode)
    {
    case 0:
        return float4(color, 1.0f);
    case 1:
        return float4(albedo.rgb, 1.0f);
    case 2:
        return float4(N.xyz, 1.0f);
    case 3:
        return float4(float3(roughness), 1.0f);
    case 4:
        return float4(float3(metallic), 1.0f);
    case 5:
        return float4(scatterColor, 1.0f);
    case 6:
        return float4(float3(positionNDC.z), 1.0f);
    }

    return float4(color, 1.0f);
}
