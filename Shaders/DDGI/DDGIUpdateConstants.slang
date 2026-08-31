cbuffer DDGIUpdateConstants : register(b0) {
    float4 randomRotation; // quaternion (xyz axis*sin(half), w = cos(half)), must match the trace pass

    int4 probeCounts; // xyz = probe grid counts, w = maxRaysPerProbe (ray buffer stride)

    uint raysPerProbe;
    float hysteresis;

    // Descriptor heap slots of the ping-pong pairs the update passes run on this frame. These are raw heap
    // indices for ResourceDescriptorHeap, not the table relative indices the trace pass' views use.
    uint irradianceReadView;
    uint irradianceWriteView;

    uint distanceReadView;
    uint distanceWriteView;

    // Ceiling on the hit distance the distance pass stores. RTXGI scales this off the probe spacing rather than
    // the ray length: the second moment is a distance squared, so a raw maxRayDistance would dominate the blend.
    float probeMaxRayDistance;
    uint updatePadding;
};

// Stride of a probe's ray range in the ray data buffer, the buffer is sized for the maximum ray count.
uint GetMaxRaysPerProbe() { return uint(probeCounts.w); }

// The probes are a dense grid, so the total is the product of the grid counts.
uint GetProbeCount() { return uint(probeCounts.x * probeCounts.y * probeCounts.z); }
