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
    float time;
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
    float height;
};

struct CulledInstance
{
    uint instanceIndex;
    float lodBlend;
};

StructuredBuffer<GrassData> grassBuffer : register(t0);
StructuredBuffer<CulledInstance> culledInstances : register(t1);

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

float3 bezier(float3 p0, float3 p1, float3 p2, float t)
{
    float3 a = lerp(p0, p1, t);
    float3 b = lerp(p1, p2, t);
    return lerp(a, b, t);
}

// Bezier Derivitive formula from GPUOpen AMD | https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/
float3 bezierDerivitive(float3 p0, float3 p1, float3 p2, float t)
{
    return 2. * (1. - t) * (p1 - p0) + 2. * t * (p2 - p1);
}

float remap1(float value, float inMin, float inMax, float outMin, float outMax)
{
    return outMin + (value - inMin) * (outMax - outMin) / (inMax - inMin);
}

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;

    CulledInstance culled = culledInstances[input.instanceID];
    GrassData grassData = grassBuffer[culled.instanceIndex];
    
    float3x3 rotationMat = rotY(grassData.rotation);
    
    float3 bladePosition = input.position;

    float2 windDirection = normalize(float2(sin(time), cos(time)));
    
    float2 bladeDirection = float2(cos(grassData.rotation), sin(grassData.rotation));
    
    // Grass leaning value
    const float leaning = 1.3f;
    float3 p0 = float3(0.0, 0.0, 0.0); //bladePosition
    float3 p1 = p0 + float3(0, grassData.height * 0.67, 0);
    float3 p2 = p0 + float3(bladeDirection.x * windDirection.x * input.sway, grassData.height, bladeDirection.y * windDirection.y * input.sway) * leaning;
    //float3 p3 = p0 + p1 + p2 * grassData.height * -leaning;
    
    bladePosition += bezier(p0, p1, p2, bladePosition.y);

    float3 up = normalize(bezierDerivitive(p0, p1, p2, bladePosition.y));
    float3 right = normalize(float3(bladeDirection.y, -bladeDirection.x, 0));

    float3 normal = cross(right, up);
    
    bladePosition = mul(rotationMat, bladePosition); // Rotate grass blade
    
    float4 vertexWorldPos = mul(model, float4(bladePosition + grassData.position, 1.0f));
    
    output.position = mul(projView, vertexWorldPos);
    output.fragPosition = vertexWorldPos.xyz;
    output.normal = normal;
    output.sway = input.sway;
    return output;
}