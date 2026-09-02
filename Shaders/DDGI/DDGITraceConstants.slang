cbuffer DDGITraceConstants : register(b0) {
    column_major float4x4 inverseView;

    float4 lightDirection;      // xyz = direction the light travels, w unused
    float4 lightColorIntensity; // rgb = color, a = intensity

    uint environmentView;

    uint numPointLights;

    uint probeDataView;
    uint tracePadding;
};
