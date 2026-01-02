cbuffer FrustumData : register(b0)
{
    float4x4 viewProj;
    float4 frustumPlanes[6];
    float3 cameraPos;
    
    // LOD distances
    float lod0;
    float lod1;
    float lod2;
    
    float lodBlendRange;
    float maxDistance;
};

struct GrassData
{
    float3 worldPosition;
    float rotation;
    float height;
};

struct CulledInstance
{
    uint instanceIndex;
    float lodBlend;
};

struct DrawIndexedCommand
{
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int BaseVertexLocation;
    uint StartInstanceLocation;
};

StructuredBuffer<GrassData> grassInstance : register(t0);
RWStructuredBuffer<CulledInstance> culledInstances : register(u0);
RWByteAddressBuffer instanceCount : register(u1);

bool IsInFrustum(float3 position, float radius)
{
    for (int i = 0; i < 6; i++)
    {
        float dist = dot(frustumPlanes[i].xyz, position) + frustumPlanes[i].w;
        if (dist < -radius)
        {
            return false;
        }
    }
    return true;
}

// The return value determines the lod 0 being the first and 3 is fully faded
float CalculateLOD(float distance)
{
    if (distance < lod0)
    {
        return 0.0;
    }
    else if (distance < lod0 + lodBlendRange)
    {
        float t = (distance - lod0) / lodBlendRange;
        return saturate(t);
    }
    else if (distance < lod1)
    {
        return 1.0;
    }
    else if (distance < lod1 + lodBlendRange)
    {
        float t = (distance - lod1) / lodBlendRange;
        return 1.0 + saturate(t);
    }
    else if (distance < lod2)
    {
        return 2.0;
    }
    else if (distance < lod2 + lodBlendRange)
    {
        float t = (distance - lod2) / lodBlendRange;
        return 2.0 + saturate(t);
    }
    else
    {
        return 3.0;
    }
}

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= 5000)
        return;
    
    GrassData instance = grassInstance[id.x];

    float cullRadius = max(instance.height, 1.0);
    
    // Frustum cull
    //if (!IsInFrustum(instance.worldPosition, cullRadius))
    //    return;
    
    //Distance cull

    float distance = length(instance.worldPosition - cameraPos);
    if (distance > 500)
        return;
    
    uint lodIndexCount = 0;
    uint startIndexLocation = 0;
    uint lod = 0;
    if (distance < 15)
    {
        lod = 0;
    }
    else if (distance < 30)
    {
        lod = 1;
    }
    else
    {
        lod = 2;
    }
    
    uint drawIndex;
    instanceCount.InterlockedAdd(lod * 4, 1, drawIndex);
    
    uint globalDrawIndex = drawIndex;
    for (uint i = 0; i < 3; i++)
    {
        if (i != lod)
        {
            globalDrawIndex += instanceCount.Load(i * 4);
        }
    }
    
    if (globalDrawIndex < 5000)
    {
        CulledInstance culled;
        culled.instanceIndex = id.x;
        culled.lodBlend = lod;
    
        culledInstances[globalDrawIndex] = culled;
    }
}