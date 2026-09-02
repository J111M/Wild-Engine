cbuffer DDGIUpdateConstants : register(b0) {
    uint irradianceReadView;
    uint irradianceWriteView;

    uint distanceReadView;
    uint distanceWriteView;

    float probeMaxRayDistance;

    uint probeDataView;

    uint2 updatePadding;
};
