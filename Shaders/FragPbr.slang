cbuffer Constants : register(b0)
{
    uint debugView;
    uint albedoView;
    uint normalView;
    uint depthView;
};

cbuffer matrices : register(b1)
{
    matrix inverseView;
    matrix inverseProjection;
}

Texture2D textures[4096] : register(t0, space0);

SamplerState linearSampler : register(s0);

float4 RestorePositionWS(float3 position)
{
    float4 positionVS = mul(inverseProjection, float4(position, 1.0));
    positionVS /= positionVS.w;

    return mul(inverseView, positionVS);
}

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

[shader("fragment")]
float4 main(VSOutput input) : SV_TARGET0
{
    float4 albedo = float4(0);
    if (albedoView != 0)
        albedo = textures[albedoView].Sample(linearSampler, input.uv);

    float4 normal = float4(0);
    if (normalView != 0)
        normal = textures[normalView].Sample(linearSampler, input.uv);

    float3 positionNDC = float3(0.0f.xxx);
    positionNDC.xy = (input.uv * 2) - 1;
    positionNDC.y *= -1;
    positionNDC.z = textures[depthView].Sample(linearSampler, input.uv).x;
    float3 positionWS = RestorePositionWS(positionNDC).rgb;

    switch (debugView)
    {
    case 0:
        return albedo;
    case 1:
        return float4(albedo.rgb, 1.0);
    case 2:
        return float4(normal.rgb * 0.5 + 0.5, 1.0);
    }

    return albedo;
}
