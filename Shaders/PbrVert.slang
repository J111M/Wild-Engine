// Full screen triangle
struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

cbuffer matrices : register(b1)
{
    column_major matrix inverseView;
    column_major matrix inverseProjection;
}

[shader("vertex")]
VSOutput main(uint vertexID: SV_VertexID)
{
    VSOutput output;

    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv.x * 2.0 - 1.0, 1.0 - output.uv.y * 2.0, 0.0, 1.0);

    return output;
}