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

VSOutput main(VSInput input)
{
    VSOutput output;
    GrassData grassData = grassBuffer[input.instanceID];
    
    float3x3 rotationMat = rotY(grassData.rotation);
    
    float3 bladePosition = input.position + grassData.position;
    bladePosition = mul(rotationMat, bladePosition); // Rotate grass blade

    output.position = mul(model, float4(bladePosition, 1.0f));
    output.sway = input.sway;
    return output;
}