struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
    float elevation : ELEVATION;
};

cbuffer Constants : register(b0)
{
    column_major float4x4 world;
    column_major float4x4 invTransposeModel;

    uint heightMapIndex;
    float noiseFade;
    float waterLevel;
};

cbuffer TextureViews : register(b1)
{
    uint sandTexture;
    uint sandNormalTexture;
    uint sandRoughTexture;
    uint sandAOTexture;

    uint rockTexture;
    uint rockNormalTexture;
    uint rockRoughTexture;
    uint rockAOTexture;

    uint noiseBlendView;
}

cbuffer Camera : register(b2)
{
     column_major float4x4 projView;
}

Texture2D textures[4096] : register(t0, space0);
SamplerState linearSampler : register(s0);

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;

    const float maxHeight = 10.0f;

    float2 texelSize = 1.0 / 512;

    float3 normal = float3(0);
    float3 tangent = float3(0, 0, 1);
    float3 bitangent = float3(1, 0, 0);

    float terrainHeight = 0.0f;
    if (heightMapIndex > 0)
    {
        terrainHeight = textures[heightMapIndex].SampleLevel(linearSampler, input.uv, 0).r;

        float hL = textures[heightMapIndex].SampleLevel(linearSampler, input.uv + float2(-texelSize.x, 0), 0).r;
        float hR = textures[heightMapIndex].SampleLevel(linearSampler, input.uv + float2(texelSize.x, 0), 0).r;
        float hD = textures[heightMapIndex].SampleLevel(linearSampler, input.uv + float2(0, -texelSize.y), 0).r;
        float hU = textures[heightMapIndex].SampleLevel(linearSampler, input.uv + float2(0, texelSize.y), 0).r;

        float scale = 2.0 * texelSize.x * 128;

        normal = normalize(float3((hL - hR) * maxHeight, scale, (hD - hU) * maxHeight));
        tangent = normalize(float3(scale, (hR - hL) * maxHeight, 0));
        bitangent = normalize(float3(0, (hU - hD) * maxHeight, scale));
    }

    float3 outputPosition = input.position;
    outputPosition.y += terrainHeight * maxHeight;

    output.worldPosition = mul(world, float4(outputPosition, 1.0f)).xyz;
    output.position = mul(projView, float4(output.worldPosition, 1.0f));
    output.normal = normal;
    output.tangent = tangent;
    output.bitangent = bitangent;
    output.uv = input.uv;
    output.elevation = outputPosition.y;
    return output;
}
