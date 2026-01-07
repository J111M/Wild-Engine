cbuffer Constants : register(b0)
{
    column_major float4x4 model;
    uint bladeId;
    float time;
    uint foo2;
    uint foo3;
};

cbuffer SceneData : register(b1)
{
    column_major float4x4 projView;
    float3 cameraPosition;
    float pad;
    float windStrength;
    float octaves;
    float frequency;
    float amplitude;
    float2 windDirection;
    float pad1;
    float pad2;
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
    float sway : SWAYIN;
};

[shader("fragment")]
float4 main(PSInput input) : SV_TARGET
{
    float3 Direction = float3(-0.1, -1.0, -0.5);
    float3 Ambient = float3(0.61, 0.32, 0.1);
    float3 Diffuse = float3(1, 0.86, 0.73);
    float3 Specular = float3(1, 0.96, 0.93);

    float3 viewDir = normalize(cameraPosition - input.fragPosition);
    float3 normal = input.normal;
    
    float3 lightDir = normalize(-Direction);
    float3 halfwayDir = normalize(lightDir + viewDir);

    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 128);
    
    float3 grassColor = float3(0.0, 0.6 + input.sway, 0.0);
    
    float3 ambient = Ambient;
    float3 diffuse = Diffuse * diff;
    float3 specular = Specular * spec;

    float3 outColor = (ambient + diffuse + specular) * grassColor;

    return float4(outColor, 1.0f);
}