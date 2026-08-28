// Constants shared by every DDGI pass. The trace pass and the update passes are fed from the same C++
// struct (Wild::DDGIRootConstants), so the ray directions the update passes rebuild are by construction
// the ones the trace pass used and no per-pass copy can drift.
cbuffer DDGIConstants : register(b0) {
    column_major float4x4 inverseView;

    float4 randomRotation; // quaternion (xyz axis*sin(half), w = cos(half))

    float4 lightDirection;      // xyz = direction the light travels, w unused
    float4 lightColorIntensity; // rgb = color, a = intensity

    float4 probeOrigin;  // xyz = grid origin, w unused
    float4 probeSpacing; // xyz = grid spacing, w unused
    int4 probeCounts;    // xyz = probe grid counts, w = maxRaysPerProbe (ray buffer stride)

    uint raysPerProbe;
    float maxRayDistance;
    float intensity;
    uint environmentView;

    uint numPointLights;
    uint IrradianceView;
    uint DistanceView;
    float hysteresis;

    // Descriptor heap slots of the ping-pong irradiance pair the update pass runs on this frame. These are raw
    // heap indices for ResourceDescriptorHeap, not the table-relative indices IrradianceView / DistanceView use.
    uint irradianceReadView;
    uint irradianceWriteView;
    uint2 ddgiPadding;
};

// Stride of a probe's ray range in the ray data buffer, the buffer is sized for the maximum ray count.
uint GetMaxRaysPerProbe() { return uint(probeCounts.w); }

// The probes are a dense grid, so the total is the product of the grid counts.
uint GetProbeCount() { return uint(probeCounts.x * probeCounts.y * probeCounts.z); }
