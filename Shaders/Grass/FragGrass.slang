cbuffer Constants : register(b0)
{
    column_major float4x4 model;
    column_major float4x4 invModel;
    uint bladeId;
    float time;
    uint foo2;
    uint foo3;
};

cbuffer SceneData : register(b1)
{
    column_major float4x4 projView;
    float3 cameraPosition;
};

struct GrassData
{
    float3 position;
    float rotation;
    float height;
};

struct CulledInstance
{
    uint instanceIndex;
    float lodBlend;
};

StructuredBuffer<GrassData> grassBuffer : register(t0);
StructuredBuffer<CulledInstance> culledInstances : register(t1);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 fragPosition : FRAGPOSITION;
    float3 normal : NORMAL;
    float bladeHeight : BHEIGHT;
};

struct PSOut
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 emissive : SV_TARGET2;
}

// Octahedral encoding
float3 OctEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0)
    {
        n.xy = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0);
    }
    return float3(n.xy * 0.5 + 0.5, 0.0);
}

[shader("fragment")]
PSOut main(PSInput input, bool isFrontFace: SV_IsFrontFace)
{
    float3 baseColor = float3(0.05f, 0.2f, 0.01f);
    float3 tipColor = float3(0.5f, 0.5f, 0.1f);

    float3 grassColor = lerp(baseColor, tipColor, input.bladeHeight);

    float3 normal = select(isFrontFace, input.normal, -input.normal);

    // Lerp with ground normal
    //float blendFactor = 1.0 - min(input.bladeHeight + 0.4, 1.0f);
    //normal = lerp(normal, float3(0, 1, 0), blendFactor);
    normal.xyz = normalize(mul((float3x3)invModel, normal));

    float roughness = 0.8f;
    float metallic = 0.0f;

    grassColor = pow(grassColor, 1.0 / 2.2);

    PSOut psOut;
    psOut.albedo = float4(grassColor.rgb, roughness);
    psOut.normal = float4(float2(OctEncode(normal.xyz).xy), 1.0 - input.bladeHeight, metallic); // Use the 1d height coordinates for SSS and no metallic
    psOut.emissive = float4(float3(0, 0, 0), input.bladeHeight + 0.3f);

    return psOut;
}
