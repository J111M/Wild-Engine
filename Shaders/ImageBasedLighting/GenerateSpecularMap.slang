cbuffer Constants : register(b0)
{
    uint environementView;
    int mipSize;
    float mipRoughness;
    int currentFace;
};

RWTexture2D<float4> OutputTexture : register(u0);
TextureCube cubeTextures[4096] : register(t0, space0);
SamplerState sampler : register(s0);

static const uint SAMPLE_COUNT = 2048u;
static const float PI = 3.14159265358979323846f;

// Code based on an article from learnopengl.com/PBR/IBL/Specular-IBL
// holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float RadicalInverse(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent-space vector to world-space sample vector
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float k = (roughness * roughness) / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

float2 IntegrateBRDF(float NdotV, float roughness)
{
    float3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0;

    float3 N = float3(0.0, 0.0, 1.0);

    for (uint i = 0; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G = GeometrySmith(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);

    return float2(A, B);
}

// Function from this blog http://alinloghin.com/articles/compute_ibl.html modfied to work with hlsl
float3 CubeCoordToWorld(uint3 cubeCoord, float2 cubemapSize)
{
    float2 texCoord = float2(cubeCoord.xy) / cubemapSize;
    texCoord = texCoord * 2.0 - 1.0; // -1..1
    texCoord.y = -texCoord.y;
    switch (cubeCoord.z)
    {
    case 0:
        return float3(1.0, -texCoord.yx); // +x
    case 1:
        return float3(-1.0, -texCoord.y, texCoord.x); // -x
    case 2:
        return float3(texCoord.x, -1.0, -texCoord.y); // +y
    case 3:
        return float3(texCoord.x, 1.0, texCoord.y); // -y
    case 4:
        return float3(texCoord.x, -texCoord.y, 1.0); // +z
    case 5:
        return float3(-texCoord.xy, -1.0); // -z
    }
    return float3(0.0);
}

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    const float2 size = float2(mipSize, mipSize);

    if (id.x >= size.x || id.y >= size.y)
        return;

    float3 worldPosition = CubeCoordToWorld(uint3(id.xy, currentFace), size);

    // tagent space from origin point
    float3 N = normalize(worldPosition);
    // assume view direction always equal to outgoing direction based on approximation from Epic Games
    float3 R = N;
    float3 V = R;

    float totalWeight = 0.0;
    float3 prefilteredColor = float3(0.0);
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        // generate sample vector towards the alignment of the specular lobe
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, mipRoughness);
        float HdotV = dot(H, V);
        float3 L = normalize(2.0 * HdotV * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            float3 envMap = float3(0, 0, 0);

            // TODO sample the environment based on mip level to get more accurate result and less artifacts
            if (environementView > 0)
                envMap = cubeTextures[environementView].Sample(sampler, L).rgb;

            prefilteredColor += envMap * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / totalWeight;
    OutputTexture[id.xy] = float4(prefilteredColor, 1.0f);
}
