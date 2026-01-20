cbuffer Constants : register(b0)
{
    uint emissiveView;
    uint albedoView;
    uint normalView;
    uint depthView;
};

cbuffer matrices : register(b1)
{
    matrix inverseView;
    matrix inverseProjection;
}

cbuffer PbrData : register(b2)
{
    float3 cameraPosition;
    float pad0;
    float3 lightDirection;
    float pad1;
}

cbuffer envData : register(b3)
{
    uint irradianceMapView;
}

Texture2D textures[4096] : register(t0, space0);
TextureCube cubeTextures[4096] : register(t0, space1);

SamplerState linearSampler : register(s0);

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
    float f = pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
    return F0 + (1.0f - F0) * f;
}

float3 FresnelSchlickRough(float3 F0, float NdotV, float roughness)
{
    float3 oneMinusR = float3(1.0f - roughness);
    float f = pow(clamp(1.0f - NdotV, 0.0f, 1.0f), 5.0f);
    return F0 + (max(oneMinusR, F0) - F0) * f;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_TARGET0
{
    float4 albedo = float4(0);
    if (albedoView != 0)
        albedo = textures[albedoView].Sample(linearSampler, input.uv);

    float4 normal = float4(0);
    if (normalView != 0)
    {
        normal = textures[normalView].Sample(linearSampler, input.uv);
        //normal.y = -normal.y; // flip y
    }

    float4 emissive = float4(0);
    if (emissiveView != 0)
        emissive = textures[emissiveView].Sample(linearSampler, input.uv);

    // srgb to linear
    albedo.rgb = pow(albedo.rgb, float3(2.2f));
    emissive.rgb = pow(emissive.rgb, float3(2.2f));

    float roughness = albedo.w;
    float metallic = normal.w;

    // Reconstruct the position from the depth
    float3 positionNDC = float3(0.0f.xxx);
    positionNDC.xy = (input.uv * 2) - 1;
    positionNDC.y *= -1;
    positionNDC.z = textures[depthView].Sample(linearSampler, input.uv).x;
    float3 positionWS = RestorePositionWS(positionNDC).xyz;

    float3 color = float3(0.0f, 0.0f, 0.0f);

    // shading setup
    float3 N = normal.xyz;
    float3 V = normalize(cameraPosition - positionWS);
    float3 R = reflect(-V, N);

    float3 L = -normalize(lightDirection);
    float3 H = normalize(V + L);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo.rgb, metallic);

    float ndotl = max(dot(N, L), 0.001f);
    float ndotv = max(dot(N, V), 0.001f);
    float hdotv = max(dot(H, V), 0.0f);
    float ndoth = max(dot(N, H), 0.001f);

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

    // direct lighting contribution
    Lo = (kD * lambertianDiffuse + specular) * ndotl; // light color

    float3 irradiance = float3(0.0f);
    if (irradianceMapView > 0)
        irradiance = cubeTextures[irradianceMapView].Sample(linearSampler, N).rgb;
    else {
        irradiance = float3(0.2f, 0.2f, 0.2f);
    }
    float3 ambient = albedo.rgb * irradiance;

    color = Lo + ambient + emissive.rgb;

    return float4(color, 1.0f);
}
