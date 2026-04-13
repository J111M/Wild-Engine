#pragma once

#include "Renderer/Resources/Buffer.hpp"

namespace Wild
{
    // Source of blas creation from https://developer.nvidia.com/rtx/raytracing/dxr/dx12-raytracing-tutorial-part-1
    struct BLASEntry
    {
        std::unique_ptr<Buffer> scratch; // Scratch memory for AS builder
        std::unique_ptr<Buffer> result;  // Where the AS is
        // std::unique_ptr<Buffer> pInstanceDesc; // Hold the matrices of the instances
    };

    struct TLASInstance
    {
        uint32_t blasIndex;
        glm::mat4 transform;
        uint32_t instanceID;
        uint32_t hitGroupIndex;
        uint8_t mask;
    };

    class AccelerationStructure
    {
      public:
        AccelerationStructure();
        ~AccelerationStructure() {};

      private:
        BLASEntry m_accelerationStructure{};
    };

    class AccelerationStructureManager : public NonCopyable
    {
      public:
        AccelerationStructureManager();
        ~AccelerationStructureManager() {};

        void AddTopLevelAS(uint32_t blasIndex, const glm::mat4& transform, uint32_t instanceID = 0, uint32_t hitGroupIndex = 0,
                           uint8_t mask = 0xFF);

        uint32_t AddBottomLevelAS(D3D12_RAYTRACING_GEOMETRY_DESC* geomDescs, uint32_t geomCount,
                                  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags =
                                      D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);

        void UpdateTransform(uint32_t instanceIndex, const glm::mat4& transform);
        void RemoveInstance(uint32_t instanceIndex);

        void UpdateTLAS();

      private:
        bool m_markDirty = false;
        bool m_tlasIsBuild = false;

        std::vector<BLASEntry> m_blasEntries{};
        std::vector<TLASInstance> m_tlasInstances{};

        // TLAS buffer
        std::unique_ptr<Buffer> m_instanceDescsBuffer;
        std::unique_ptr<Buffer> m_tlasScratch;
        std::unique_ptr<Buffer> m_tlasUpdateScratch;
        std::unique_ptr<Buffer> m_tlasResult;
        UINT64 m_tlasResultSize = 0;
    };
} // namespace Wild
