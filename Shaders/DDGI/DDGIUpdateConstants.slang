// Include file since multiple shaders share the same root constant
cbuffer DDGIUpdateConstants : register(b0) {
    uint irradianceReadView;
    uint irradianceWriteView;

    uint distanceReadView;
    uint distanceWriteView;

    uint probeDataView;

    uint3 updatePadding;
};
