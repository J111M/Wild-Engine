struct VertexOutput {
    float4 clipSpacePosition : SV_POSITION;
    float3 worldSpacePosition : POSITION0;
    float3 worldSpaceNormal : NORMAL0;
    float rootHeight : BLENDWEIGHT0;
    float height : BLENDWEIGHT1;
};

static const int GROUP_SIZE = 128;
static const int GRASS_VERT_COUNT = 256;
static const int GRASS_PRIM_COUNT = 192;

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void main(
    uint gtid: SV_GroupThreadID,
    uint gid: SV_GroupID,
    out indices uint3 tris[GRASS_PRIM_COUNT],
    out vertices VertexOutput verts[GRASS_VERT_COUNT]
) {

}