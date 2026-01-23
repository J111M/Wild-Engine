cbuffer Constants : register(b0) {
    float2 textureSize;
    float2 chunkPosition;
};

// Terrain heigth map
RWTexture2D<float> OutputTexture : register(u0);

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 id: SV_DispatchThreadID)
{
    if (id.x > textureSize.x || id.y > textureSize.y)
        return;

    // Output into texture
    OutputTexture.Store(id.xy, id.x * id.y);
}
