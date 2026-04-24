struct RayPayload {
    float3 color;
};

cbuffer CameraConstants : register(b0) {
    float4x4 viewInverse;
    float4x4 projInverse;
};

RaytracingAccelerationStructure tlas : register(t0);

RWTexture2D<float4> output : register(u0);


[shader("raygeneration")]
void RayGen() {
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    float2 uv = (float2(launchIndex) + 0.5) / float2(launchDim);
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float4 target = mul(projInverse, float4(ndc, 1.0, 1.0));
    float3 dir = mul(viewInverse, float4(normalize(target.xyz / target.w), 0.0)).xyz;
    float3 origin = mul(viewInverse, float4(0, 0, 0, 1)).xyz;

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = dir;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    RayPayload payload;
    payload.color = float3(0, 0, 0);

    TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    output[launchIndex] = float4(float3(1,0,1), 1.0);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, BuiltInTriangleIntersectionAttributes attribs) {
    payload.color = float3(0.0, 1.0, 0.0);
}

[shader("miss")]
void Miss(inout RayPayload payload) {
    payload.color = float3(1.0, 0.0, 0.0);
}