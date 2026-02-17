struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
};

struct PSOut
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 emissive : SV_TARGET2;
}

cbuffer Constants : register(b0)
{
    column_major float4x4 model;
    column_major float4x4 invTransposeModel;
    uint albedoView;
    uint normalView;
    uint roughnessMetallicView;
    uint emissiveView;
};

Texture2D textures[4096] : register(t0, space0);
SamplerState anisotropySampler : register(s0);

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
    float4 albedo = float4(0, 0, 0, 0);
    if (albedoView != 0) { albedo = textures[NonUniformResourceIndex(albedoView)].Sample(anisotropySampler, input.uv); }

    float4 normal = float4(0, 0, 0, 0);
    if (normalView != 0)
    {
        normal.xyz = textures[NonUniformResourceIndex(normalView)].Sample(anisotropySampler, input.uv).xyz;
        normal.xyz = normal.xyz * 2.0 - 1.0;
        //normal.xyz = mul((float3x3)invTransposeModel, normal.xyz);

        float3 geoNormal = normalize(mul((float3x3)invTransposeModel, input.normal.xyz));

        float4 tangent = input.tangent;

        float3 T = normalize(mul((float3x3)model, tangent.xyz));
        T = normalize(T - dot(T, geoNormal) * geoNormal);
        float3 B = cross(geoNormal, T) * tangent.w;

        float3x3 TBN = float3x3(T, B, geoNormal);

        normal.xyz = normalize(mul((normal.xyz), TBN));
    }
    else {
        normal.xyz = mul((float3x3)invTransposeModel, input.normal.xyz);
        normal.xyz = normalize(normal.xyz * 2.0 - 1.0);
    }

    

    float4 emissive = float4(0, 0, 0, 0);
    if (emissiveView != 0)
    {
        emissive = textures[NonUniformResourceIndex(emissiveView)].Sample(anisotropySampler, input.uv);
    }

    float roughness = 0.0f;
    float metallic = 0.0f;
    
    if (roughnessMetallicView != 0)
    {
        float2 rm = textures[NonUniformResourceIndex(roughnessMetallicView)].Sample(anisotropySampler, input.uv).gb;
        roughness = rm.x;
        metallic = rm.y;
    }

    PSOut psOut;
    psOut.albedo = float4(albedo.rgb, roughness);
    psOut.normal = float4(float2(OctEncode(normal.xyz).xy), 0.0f, metallic);
    psOut.emissive = float4(emissive.rgb, 1.0f);

    return psOut;
}
