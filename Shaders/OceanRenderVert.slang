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
    float3 worlPosition : WORLDPOS;
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float foam : FOAM;
    float waveHeight : WHEIGHT;
};

cbuffer Constants : register(b0)
{
    column_major float4x4 modelMatrix;
    float4 cameraPosition;
    float4 directionIntensity;

    uint displacementMapView;
    uint slopeMapView;
    uint irradianceView;
    uint specularView;
}

cbuffer Camera : register(b1)
{
    column_major float4x4 projViewMatrix;
    column_major float4x4 invModel;
}

cbuffer OceanRenderData : register(b2) {
    float4 uvScalars;

    float normalScalar;
    float reflectionScalar;
    float waveHeightScalar;
    float airBubbleDensity;

    float4 waterScatterColor;
    float4 airBubblesColor;
    float4 foamColor;

    float peakScatterStrength;
    float scatterStrength;
    float scatterShadowStrength;
    float pad;
};

Texture3D textures[4096] : register(t0, space0);
TextureCube textureCubes[4096] : register(t0, space1);
SamplerState anisotropySampler : register(s0);

#define HEIGHT_SCALE 100.0f

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;

    float4 worldPos = mul(modelMatrix, float4(input.position, 1.0f));

    float4 displacement1 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(worldPos.xz / uvScalars.x, 0), 0);
    float4 displacement2 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(worldPos.xz / uvScalars.y, 1), 0);
    float4 displacement3 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(worldPos.xz / uvScalars.z, 2), 0);
    float4 displacement4 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(worldPos.xz / uvScalars.w, 3), 0);
    float4 displacement = displacement1 + displacement2 + displacement3;// + displacement4;

    float3 displacedPosition = input.position + (displacement.xyz * HEIGHT_SCALE);

    output.waveHeight = displacement.y;
    output.foam = displacement3.w;
    worldPos = mul(modelMatrix, float4(displacedPosition.xyz, 1.0f));
    output.worlPosition = worldPos.xyz;
    output.position = mul(projViewMatrix, worldPos);
    output.color = input.color;
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}
