#include "../Helper/Common.slang"
#include "../Helper/Rng.slang"

cbuffer RootConsant : register(b0)
{
    float textureSize;
    uint cellCount;
}

#define CELL_COUNT 4

RWTexture3D<float4> OutputTexture : register(u0);

// Tileable perlin noise from Stefan Gustavson (stegu@itn.liu.se)
static const uint perm[512] = {
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142, 8,   99,
    37,  240, 21,  10,  23,  190, 6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,
    57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166,
    77,  146, 158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143, 54,
    65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159, 86,
    164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212,
    207, 206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,
    221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104,
    218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,  145, 235, 249, 14,  239, 107,
    49,  192, 214, 31,  181, 199, 106, 157, 184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,
    222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180,

    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142, 8,   99,
    37,  240, 21,  10,  23,  190, 6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,
    57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166,
    77,  146, 158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143, 54,
    65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159, 86,
    164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212,
    207, 206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,
    221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104,
    218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,  145, 235, 249, 14,  239, 107,
    49,  192, 214, 31,  181, 199, 106, 157, 184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,
    222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180
};

// 3D Gradient table
static const float3 grad3[16] = { float3(1, 1, 0),
                                  float3(-1, 1, 0),
                                  float3(1, -1, 0),
                                  float3(-1, -1, 0),
                                  float3(1, 0, 1),
                                  float3(-1, 0, 1),
                                  float3(1, 0, -1),
                                  float3(-1, 0, -1),
                                  float3(0, 1, 1),
                                  float3(0, -1, 1),
                                  float3(0, 1, -1),
                                  float3(0, -1, -1),
                                  float3(1, 1, 0),
                                  float3(-1, 1, 0),
                                  float3(0, -1, 1),
                                  float3(0, -1, -1) };

// Permutation lookup for picking a number from the table
uint Perm2(uint i) { return perm[i & 255]; }

uint GradIndex(uint x, uint y, uint z, uint px, uint py, uint pz)
{
    uint xi = x % px;
    uint yi = y % py;
    uint zi = z % pz;
    return Perm2(xi + Perm2(yi + Perm2(zi))) & 15;
}

// Sample the gradients and compute the gradients dot residualvectors the final noise value needs to be rescaled because the
// gradients are bigger than unit length This will match it with clasic perlin values
float Grad(uint idx, float3 diff) { return dot(grad3[idx], diff); }

float Fade(float t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }

// Function provided by claude
// Promt: Provide me with a tileable perlin noise function based on the implementation from Stefan Gustavson
// https://github.com/MethanePowered/PerlinNoise/blob/main/Sources/noise1234.cpp#L109
float PeriodicPerlinNoise(float3 p, uint3 period)
{
    // Integer cell coords
    uint3 i = uint3(floor(p));

    // Fractional
    float3 f = frac(p);

    // Fade
    float3 u = float3(Fade(f.x), Fade(f.y), Fade(f.z));

    // Gradient indices for the cell corners
    uint aaa = GradIndex(i.x, i.y, i.z, period.x, period.y, period.z);
    uint baa = GradIndex(i.x + 1, i.y, i.z, period.x, period.y, period.z);
    uint aba = GradIndex(i.x, i.y + 1, i.z, period.x, period.y, period.z);
    uint bba = GradIndex(i.x + 1, i.y + 1, i.z, period.x, period.y, period.z);
    uint aab = GradIndex(i.x, i.y, i.z + 1, period.x, period.y, period.z);
    uint bab = GradIndex(i.x + 1, i.y, i.z + 1, period.x, period.y, period.z);
    uint abb = GradIndex(i.x, i.y + 1, i.z + 1, period.x, period.y, period.z);
    uint bbb = GradIndex(i.x + 1, i.y + 1, i.z + 1, period.x, period.y, period.z);

    float x1, x2, y1, y2;
    x1 = lerp(Grad(aaa, f), Grad(baa, f - float3(1, 0, 0)), u.x);
    x2 = lerp(Grad(aba, f - float3(0, 1, 0)), Grad(bba, f - float3(1, 1, 0)), u.x);
    y1 = lerp(x1, x2, u.y);

    x1 = lerp(Grad(aab, f - float3(0, 0, 1)), Grad(bab, f - float3(1, 0, 1)), u.x);
    x2 = lerp(Grad(abb, f - float3(0, 1, 1)), Grad(bbb, f - float3(1, 1, 1)), u.x);
    y2 = lerp(x1, x2, u.y);

    // Remap the output value from -1, 1 to 0, 1
    return saturate((lerp(y1, y2, u.z) + 1.0) * 0.5);
}

// Worley noise base from https://thebookofshaders.com/12/ modified to work with hlsl and my case
float WorleyNoise(float3 p, float period)
{
    float3 i = floor(p);
    float3 f = fract(p);
    float minDist = 1.0f;
    int iperiod = int(round(period));

    for (int z = -1; z <= 1; z++) 
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                int3 neighbor = int3(x, y, z);
                // Wrap cell index into [0, period)
                int3 cell = int3(i) + neighbor;
                cell = ((cell % iperiod) + iperiod) % iperiod;

                uint3 h = murmurHash33(uint3(cell));
                float3 hashVal = asfloat(h & 0x007fffffu | 0x3f800000u) - 1.0f;

                float dist = length(f - (hashVal + float3(neighbor)));
                minDist = min(minDist, dist);
            }
        }
    }

    return minDist;
}

[shader("compute")]
[numthreads(8, 8, 8)]
void main(uint3 id: SV_DispatchThreadID)
{
    // Normalize coordinates to 0, 1
    float3 uvw = (float3(id) + 0.5) / float3(textureSize);
    float3 positionScaled = uvw * float(CELL_COUNT);

    uint3 period = uint3(CELL_COUNT, CELL_COUNT, CELL_COUNT);

    // Sample the Worley noise at different frequencies and invert it
    float lowFreqWorley = 1.0 - WorleyNoise(positionScaled, float(CELL_COUNT));
    float midFreqWorley = 1.0 - WorleyNoise(positionScaled * 2.0f, float(CELL_COUNT) * 2.0f);
    float highFreqWorley = 1.0 - WorleyNoise(positionScaled * 4.0f, float(CELL_COUNT) * 4.0f);

    // Store different frequencies of noise in the 3D texture
    float4 noiseOctaves = float4(0);
    noiseOctaves.x = saturate(remap( PeriodicPerlinNoise(positionScaled, period), lowFreqWorley - 1.0, 1.0, 0.0, 1.0)); // Perlin combined with worly noise
    noiseOctaves.y = lowFreqWorley;
    noiseOctaves.z = midFreqWorley;
    noiseOctaves.w = highFreqWorley;

    OutputTexture[id] = noiseOctaves;
}
