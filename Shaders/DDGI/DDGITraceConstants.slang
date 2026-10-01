cbuffer DDGITraceConstants : register(b0) {
    float4 lightDirection;      // xyz = direction the light travels, w unused
    float4 lightColorIntensity; // rgb = color, a = intensity

    uint environmentView;

    uint numPointLights;

    uint probeDataView;
    float emissiveIntensity;
};
