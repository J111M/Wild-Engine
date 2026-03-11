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
    float3 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

cbuffer Constants : register(b0)
{
    column_major float4x4 world;
    column_major float4x4 invTransposeModel;
    uint displacementMapView;
    uint slopeMapView;
}

Texture3D textures[4096] : register(t0, space0);
TextureCube textureCubes[4096] : register(t0, space1);
SamplerState anisotropySampler : register(s0);

#define HEIGHT_SCALE 1.0f

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;

    float3 displacement1 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.position.xz / 1500.0f, 0), 0).xyz;
    float3 displacement2 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.position.xz / 250.0f, 1), 0).xyz;
    float3 displacement3 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.position.xz / 17.0f, 2), 0).xyz;
    float3 displacement4 = textures[displacementMapView].SampleLevel(anisotropySampler, float3(input.position.xz / 5.0f, 3), 0).xyz;
    float3 displacement = displacement1 + displacement2 + displacement3 + displacement4;

    float3 displacedPosition = input.position + (displacement * HEIGHT_SCALE);

    output.position = mul(world, float4(displacedPosition, 1.0f));
    output.color = input.color;
    output.normal = input.normal;
    output.uv = input.uv;
    return output;
}