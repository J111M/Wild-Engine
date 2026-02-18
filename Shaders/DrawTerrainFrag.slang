struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
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

cbuffer TextureViews : register(b1)
{
    uint sandTexture;
    uint sandNormalTexture;
    uint sandRoughTexture;
    uint sandAOTexture;

    uint grassTexture;
    uint grassNormalTexture;
    uint grassRoughTexture;
    uint grassAOTexture;

    uint rockTexture;
    uint rockNormalTexture;
    uint rockRoughTexture;
    uint rockAOTexture;

    uint noiseBlendView;
}

Texture2D textures[4096] : register(t0, space0);
SamplerState anisotropySampler : register(s0);
SamplerState anisotropy2Sampler : register(s1);

// Octahedral encoding
float3 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) { n.xy = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0); }
    return float3(n.xy * 0.5 + 0.5, 0.0);
}

[shader("fragment")]
PSOut main(PSInput input)
{

    float3 albedo = float3(0.0f);
    float noise = textures[NonUniformResourceIndex(heightMapIndex)].Sample(anisotropySampler, input.uv).r;

    float t = 0.8f;
    float noiseBlend = textures[NonUniformResourceIndex(noiseBlendView)].Sample(anisotropy2Sampler, input.uv).r;
    noiseBlend = noiseBlend * 2.0f - 1.0f; // Remap -1, 1

    float4 grassColor = textures[NonUniformResourceIndex(grassTexture)].Sample(anisotropy2Sampler, input.uv * 20.0);
    float3 grassNormal = textures[NonUniformResourceIndex(grassNormalTexture)].Sample(anisotropy2Sampler, input.uv * 20.0).rgb * 2.0 - 1.0f;
    
    float4 sandColor = textures[NonUniformResourceIndex(sandTexture)].Sample(anisotropy2Sampler, input.uv * 20.0);
    float4 rockColor = textures[NonUniformResourceIndex(rockTexture)].Sample(anisotropy2Sampler, input.uv * 20.0);
    float3 slopeColor = sandColor.rgb;

    albedo.rgb = float3(0.0f);

    float3 T = normalize(input.tangent);
    float3 B = normalize(input.bitangent);
    float3 N = normalize(mul((float3x3)invTransposeModel, input.normal.xyz));

    float3x3 TBN = float3x3(T, B, N);

    float3 normal = normalize(mul((grassNormal.xyz), TBN));

    // Slope steepness of the terrain
    float slope = 1.0f - input.normal.y;
    float blendAmount = 0.0f;
    float maskedSlope = slope + noise * 0.12f;

    if (maskedSlope < 0.2)
    {
        blendAmount = maskedSlope / 0.2f;
        albedo = lerp(grassColor.rgb, slopeColor.rgb, blendAmount);
    }

    if ((maskedSlope < 0.7) && (maskedSlope >= 0.2f))
    {
        blendAmount = (maskedSlope - 0.2f) * (1.0f / (0.7f - 0.2f));
        albedo = lerp(slopeColor.rgb, rockColor.rgb, blendAmount);
    }

    if (maskedSlope >= 0.7) { albedo = rockColor.rgb; }

    float roughness = 0.8f;
    float metallic = 0.0f;

    PSOut psOut;
    psOut.albedo = float4(albedo.rgb, roughness);
    psOut.normal = float4(float2(OctEncode(normal.xyz).xy), 0.0f, metallic);
    psOut.emissive = float4(float3(0.xxx), 1.0f);

    return psOut;
}
