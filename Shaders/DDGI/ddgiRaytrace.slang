struct RayPayload {
    float3 color;
};

cbuffer CameraConstants : register(b0) {
    column_major float4x4 viewInverse;
    column_major float4x4 projInverse;
};

RaytracingAccelerationStructure tlas : register(t0);

RWTexture2D<float4> output : register(u0);


[shader("raygeneration")]
void RayGen() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    float2 uv = (float2(launchIndex) + 0.5) / float2(launchDim);
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

    float4 target = mul(projInverse, float4(ndc, 1.0, 1.0));
    float3 dirVS = target.xyz / target.w;

    float3 origin = mul(viewInverse, float4(0, 0, 0, 1)).xyz;
    float3 dir = normalize(mul(viewInverse, float4(dirVS, 0.0)).xyz);

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    RayPayload payload;
    payload.color = float3(0, 0, 0);

    TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    output[launchIndex] = float4(payload.color, 1.0);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, BuiltInTriangleIntersectionAttributes attribs) {
    payload.color = float3(0.0, 1.0, 0.0);
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    payload.color = float3(1.0, 0.0, 0.0);
}