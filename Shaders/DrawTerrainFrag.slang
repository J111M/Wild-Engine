struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSOut
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 emissive : SV_TARGET2;
}

cbuffer Constants : register(b0)
{
    column_major float4x4 world;
    column_major float4x4 invTransposeModel;
    uint heightMapIndex;
};

Texture2D textures[4096] : register(t0, space0);
SamplerState anisotropySampler : register(s0);

// Octahedral encoding
float3 OctEncode(float3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0);
    }
    return float3(n.xy * 0.5 + 0.5, 0.0);
}

[shader("fragment")]
PSOut main(PSInput input)
{

    float3 albedo = float3(0.0f);
    float noise = textures[NonUniformResourceIndex(heightMapIndex)].Sample(anisotropySampler, input.uv).r;

    float t = 0.8f;
    float3 terrainColor = float3(0.45f, 0.5f, 0.1f);
    float3 slopeColor = float3(0.15f, 0.5f, 0.1f);
    float3 rockColor = float3(0.55f, 0.1f, 0.4f);
    albedo.rgb = lerp(float3(noise), terrainColor, t);

    float4 normal = float4(0);
    normal.xyz = normalize(mul((float3x3)invTransposeModel, input.normal.xyz));

    // Slope steepness of the terrain
    float slope = 1.0f - input.normal.y;
    float blendAmount = 0.0f;
    if (slope < 0.2)
    {
        blendAmount = slope / 0.2f;
        albedo = lerp(terrainColor, slopeColor, blendAmount);
    }

    if ((slope < 0.7) && (slope >= 0.2f))
    {
        blendAmount = (slope - 0.2f) * (1.0f / (0.7f - 0.2f));
        albedo = lerp(slopeColor, rockColor, blendAmount);
    }

    if (slope >= 0.7) 
    {
        albedo = rockColor;
    }

    float roughness = 0.8f;
    float metallic = 0.0f;
    
    PSOut psOut;
    psOut.albedo = float4(albedo.rgb, roughness);
    psOut.normal = float4(float2(OctEncode(normal.xyz).xy), 0.0f, metallic);
    psOut.emissive = float4(float3(0.xxx), 1.0f);

    return psOut;
}
