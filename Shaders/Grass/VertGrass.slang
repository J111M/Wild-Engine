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
    float bladeHeight : BHEIGHT;
};

cbuffer Constants : register(b0)
{
    column_major float4x4 model;
    uint bladeId;
    float time;
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

float3x3 rotY(float a)
{
    float c = cos(a);
    float s = sin(a);
    return float3x3(c, 0, s, 0, 1, 0, -s, 0, c);
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

// Bezier Derivitive formula from GPUOpen AMD |
// https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/
float3 bezierDerivitive(float3 p0, float3 p1, float3 p2, float t)
{
    return 2. * (1. - t) * (p1 - p0) + 2. * t * (p2 - p1);
}

float remap1(float value, float inMin, float inMax, float outMin, float outMax)
{
    return outMin + (value - inMin) * (outMax - outMin) / (inMax - inMin);
}

void MakePersistentLength(in float3 v0, inout float3 v1, inout float3 v2, in float height)
{
    // Persistent length
    float3 v01 = v1 - v0;
    float3 v12 = v2 - v1;
    float lv01 = length(v01);
    float lv12 = length(v12);

    float L1 = lv01 + lv12;
    float L0 = length(v2 - v0);
    float L = (2.0f * L0 + L1) / 3.0f; // http://steve.hollasch.net/cgindex/curves/cbezarclen.html

    float ldiff = height / L;
    v01 = v01 * ldiff;
    v12 = v12 * ldiff;
    v1 = v0 + v01;
    v2 = v1 + v12;
}

uint murmurHash12(uint2 src) {
    const uint M = 0x5bd1e995u;
    uint h = 1190494759u;
    src *= M; src ^= src >> 24u; src *= M;
    h *= M; h ^= src.x; h *= M; h ^= src.y;
    h ^= h >> 13u; h *= M; h ^= h >> 15u;
    return h;
}

float hash12(float2 p)
{
    uint h = murmurHash12(asuint(p));
    return asfloat(h & 0x007fffffu | 0x3f800000u) - 1.0;
}

float noise(float2 p)
{
    float2 i = floor(p);

    float2 f = fract(p);
    float2 u = smoothstep(float2(0.0), float2(1.0), f);

    float val = lerp(lerp(hash12(i + float2(0.0, 0.0)), 
                        hash12(i + float2(1.0, 0.0)), u.x),
                     lerp(hash12(i + float2(0.0, 1.0)), 
                        hash12(i + float2(1.0, 1.0)), u.x), u.y);
    return val * 2.0 - 1.0;
}

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;

    CulledInstance culled = culledInstances[input.instanceID];
    GrassData grassData = grassBuffer[culled.instanceIndex];

    float3x3 rotationMat = rotY(grassData.rotation);

    float3 initialBladeHeight = mul(rotationMat, input.position) + grassData.position.xyz;
    float3 bladePosition = initialBladeHeight;

    float2 bladeDirection = float2(cos(grassData.rotation), sin(grassData.rotation));

    // Wind calculation
    float2 windDir = normalize(windDirection);
    float2 windUV = (bladePosition.xz) * frequency + windDirection * time;
    float windNoise = noise(windUV * octaves) * amplitude;
    float3 windOffset = float3(windDir.x, 0, windDir.y) * windNoise * windStrength; // ((windNoise - 0.5f) * 2.0f)

    // Grass leaning value
    const float leaning = 0.7f;
    float3 v0 = bladePosition; // Fixed position of the grass blade
    float3 v1 = v0 + float3(0.0f, grassData.height * 0.67f, 0.0f); // Up vector
    float3 v2 = v1 + float3(bladeDirection.x, grassData.height, bladeDirection.y) * leaning;

    v2 += windOffset;
    // v2 += GetWindOffset(p0.xy, DynamicConst.shaderTime);

    // MakePersistentLength(v0, v1, v2, input.position.y);

    bladePosition = bezier(v0, v1, v2, initialBladeHeight.y);

    float3 up = normalize(bezierDerivitive(v0, v1, v2, initialBladeHeight.y));
    float3 right = normalize(float3(bladeDirection.y, 0.0f, -bladeDirection.x));

    float3 worldSpaceNormal = cross(right, up);

    float4 vertexWorldPos = mul(model, float4(bladePosition, 1.0f));

    output.position = mul(projView, vertexWorldPos);
    output.fragPosition = vertexWorldPos.xyz;
    output.normal = worldSpaceNormal;
    output.bladeHeight = initialBladeHeight.y;
    return output;
}
