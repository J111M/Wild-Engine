#include "Helper/SceneData.slang"

ConstantBuffer<SceneCameraData> sceneCamera : register(b0);

struct VSInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(sceneCamera.viewProjection, float4(input.position, 1.0f));
    output.color = input.color;
    return output;
}
