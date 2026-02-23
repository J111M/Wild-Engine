struct DrawIndexedCommand
{
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int BaseVertexLocation;
    uint StartInstanceLocation;
};

struct CulledInstance
{
    uint instanceIndex;
    float lodBlend;
};