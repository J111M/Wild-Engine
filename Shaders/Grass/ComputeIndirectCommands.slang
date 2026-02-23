#include "GrassCommon.slang"

RWByteAddressBuffer instanceCount : register(u0);
RWStructuredBuffer<DrawIndexedCommand> DrawCommands : register(u1);

[shader("compute")]
[numthreads(3, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    const uint IndexCount[3] = { 21, 15, 9 };
    const uint BaseVertex[3] = { 0, 9, 16 };
    const uint BaseIndex[3] = { 0, 21, 36 };
    
    DrawCommands[id.x].IndexCountPerInstance = IndexCount[id.x];
    DrawCommands[id.x].InstanceCount = instanceCount.Load(id.x * 4); // 4 byte uint
    DrawCommands[id.x].StartIndexLocation = BaseIndex[id.x];
    DrawCommands[id.x].BaseVertexLocation = BaseVertex[id.x];

    uint instanceOffset = 0;
    for (uint i = 0; i < id.x; i++)
    {
        instanceOffset += instanceCount.Load(i * 4);
    }

    DrawCommands[id.x].StartInstanceLocation = instanceOffset;
}