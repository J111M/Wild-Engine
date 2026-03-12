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
    float4 cameraPosition;

    float4 uvScalars;

    uint displacementMapView;
    uint slopeMapView;
}

cbuffer Camera : register(b1) {
    column_major float4x4 projViewMatrix;
    column_major float4x4 modelMatrix;
    column_major float4x4 invModel;
}

Texture3D textures[4096] : register(t0, space0);
TextureCube textureCubes[4096] : register(t0, space1);
SamplerState anisotropySampler : register(s0);

#define HEIGHT_SCALE 15.0f

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;

    float4 displacement1 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.x, 0), 0);
    float4 displacement2 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.y, 0.33f), 0);
    float4 displacement3 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.z, 0.66f), 0);
    float4 displacement4 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.uv * uvScalars.w, 1.0f), 0);
    float4 displacement = displacement1 + displacement2 + displacement3 + displacement4;

    float3 displacedPosition = input.position + (displacement.xyz * HEIGHT_SCALE);

    output.waveHeight = displacement.y;
    output.foam = displacement.w;
    float4 worldPos = mul(modelMatrix, float4(displacedPosition.xyz, 1.0f));
    output.worlPosition = worldPos.xyz;
    output.position = mul(projViewMatrix, worldPos);
    output.color = input.color;
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}
