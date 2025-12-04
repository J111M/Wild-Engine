struct VSInput
{
    float3 position : POSITION;
    float texCoord : COORDS;
    float sway : SWAY;
    uint instanceID : SV_InstanceID;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 fragPosition : FRAGPOSITION;
    float3 normal : NORMAL;
    float sway : SWAYIN;
};

cbuffer Constants : register(b0)
{
    matrix model;
    uint bladeId;
    uint foo1;
    uint foo2;
    uint foo3;
};

cbuffer SceneData : register(b1)
{
    float4x4 projView;
    float3 cameraPosition;
};

struct GrassData
{
    float3 position;
    float rotation;
};

StructuredBuffer<GrassData> grassBuffer : register(t0);

float3x3 rotY(float a)
{
    float c = cos(a);
    float s = sin(a);
    return float3x3(
         c, 0, s,
         0, 1, 0,
        -s, 0, c
    );
}

float3 toBezier(float yValue)
{
    float3 pZero = float3(0.0, 0.0, 0.0);
    float3 pOne = float3(0.0, 0.5, 0.0);
    float3 pTwo = float3(0.1, 0.8, 0.1);
    return (1 - yValue) * ((1 - yValue) * pZero + yValue * pOne) + yValue * ((1 - yValue) * pOne + yValue * pTwo);
}
float3 toBezierDerivative(float yValue)
{
    float3 pZero = float3(0.0, 0.0, 0.0);
    float3 pOne = float3(0.0, 0.5, 0.0);
    float3 pTwo = float3(0.0, 0.8, 0.1);
    return 2 * (1 - yValue) * (pOne - pZero) + 2 * yValue * (pTwo - pOne);
}

VSOutput main(VSInput input)
{
    VSOutput output;
    GrassData grassData = grassBuffer[input.instanceID];
    
    float3x3 rotationMat = rotY(grassData.rotation);
    
    float3 bladePosition = input.position;
    bladePosition = mul(rotationMat, bladePosition); // Rotate grass blade
    
    bladePosition += toBezier(bladePosition.y);

    float3 up = normalize(toBezierDerivative(bladePosition.y));
    float3 right = float3(1, 0, 0);

    float3 normal = normalize(cross(up, right));
    
    float4 vertexWorldPos = mul(model, float4(bladePosition + grassData.position, 1.0f));
    
    output.position = mul(projView, vertexWorldPos);
    output.fragPosition = vertexWorldPos.xyz;
    output.normal = normal;
    output.sway = input.sway;
    return output;
}