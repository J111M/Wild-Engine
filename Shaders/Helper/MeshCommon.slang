struct MeshletPayload {
    uint32_t groupMeshletOffset;
    uint8_t meshletIndices[64];
};

groupshared MeshletPayload sharedPayload;