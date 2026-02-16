struct VertexOut {
    float4 position : SV_POSITION;      // clip space
    float3 fragPosition : FRAGPOSITION; // world space
    float3 normal : NORMAL;
    float bladeHeight : BHEIGHT;
};

struct GrassData
{
    float3 position;
    float rotation;
    float height;
};

StructuredBuffer<GrassData> grassBuffer : register(t0);

Texture2D textures[4096] : register(t2, space0);
SamplerState anisotropySampler : register(s0);

static const int GROUP_SIZE = 128;
#define MAX_MESH_VERTS 256
#define MAX_MESH_PRIMS 256

//struct TaskPayload
//{
//    uint bladeIndices[GROUP_SIZE];
//    uint bladeCount;
//    uint lod;
//};

//static const uint LOD_SEGMENTS[3]    = { 4, 3, 2 };
//static const uint LOD_VERTS[3]       = { 9, 7, 5 };
//static const uint LOD_TRIS[3]        = { 7, 5, 3 };
//static const uint LOD_MAX_BLADES[3]  = { 28, 36, 51 };

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main(
    uint gtid: SV_GroupThreadID,
    uint gid: SV_GroupID,
    out indices uint3 tris[MAX_MESH_PRIMS],
          out vertices VertexOut verts[MAX_MESH_VERTS]
) {
    //uint bladeCount = 25000;
    //uint lod = 0;

    //uint segments = LOD_SEGMENTS[lod];
    //uint vertsPerBlade = LOD_VERTS[lod];
    //uint trisPerBlade = LOD_TRIS[lod];

    //uint totalVerts = bladeCount * vertsPerBlade;
    //uint totalTris = bladeCount * trisPerBlade;
}