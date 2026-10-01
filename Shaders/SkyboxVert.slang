#include "Helper/SceneData.slang"

cbuffer Constants : register(b0)
{
    uint skyView;
    uint skyCubeView;
};

ConstantBuffer<SceneCameraData> sceneCamera : register(b1);

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
    float3 UVW : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.UVW = input.position;

    // Only rotate the cube with the camera so the skybox stays centered around it
    float3 viewDirection = mul((float3x3)sceneCamera.view, input.position);

    output.position = mul(sceneCamera.projection, float4(viewDirection, 1.0f));
    output.position = output.position.xyww; 
    return output;
}
