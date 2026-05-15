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
        uint32_t blasIndex{};
        glm::mat4 transform{};
        uint32_t instanceID{};
        uint32_t hitGroupIndex{};
        uint8_t mask{};
    };

    struct MeshInstanceInfo
    {
        uint32_t vbHandle{};
        uint32_t ibHandle{};

        uint32_t albedoView{};
        uint32_t normalView{};
        uint32_t roughnessMetallicView{};
        uint32_t emissiveView{};

        uint32_t ambientOcclussionView{};
        float roughness{};
        float metallic{};
        float emissiveStrength{};
    };

    class AccelerationStructureManager : public NonCopyable
    {
      public:
        AccelerationStructureManager();
        ~AccelerationStructureManager() {};

        /// <summary>
        /// Adds mesh info into the mesh info buffer
        /// </summary>
        /// <param name="infoDesc">Mesh description contain srv's to the vertex index and material buffer</param>
        /// <returns>Instance ID of the mesh</returns>
        uint32_t AddMeshInfo(const MeshInstanceInfo& infoDesc);

        uint32_t AddTopLevelAS(uint32_t blasIndex, const glm::mat4& transform, uint32_t instanceID, uint32_t hitGroupIndex = 0,
                               uint8_t mask = 0xFF);

        uint32_t AddBottomLevelAS(D3D12_RAYTRACING_GEOMETRY_DESC* geomDescs, uint32_t geomCount,
                                  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags =
                                      D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);

        void UpdateTransform(uint32_t instanceIndex, const glm::mat4& transform);
        void RemoveInstance(uint32_t instanceIndex);

        void UpdateTLAS();

        D3D12_GPU_VIRTUAL_ADDRESS GetTLASAddress() const
        { return m_tlasResult ? m_tlasResult->GetBuffer()->GetGPUVirtualAddress() : 0; }

        std::shared_ptr<Buffer> GetMeshIdBuffer() const { return m_meshIdBuffer; }

      private:
        // Update capacity of the mesh id buffer
        // void Grow(uint32_t newCapacity);

        static constexpr uint32_t m_initialCapacity = 1024;

        bool m_markDirty = false;
        bool m_tlasIsBuild = false;

        std::vector<BLASEntry> m_blasEntries{};
        std::vector<TLASInstance> m_dynamicTlasInstances{};

        // TLAS buffer
        std::unique_ptr<Buffer> m_instanceDescsBuffer;
        std::unique_ptr<Buffer> m_tlasScratch;
        std::unique_ptr<Buffer> m_tlasUpdateScratch;
        std::unique_ptr<Buffer> m_tlasResult;
        UINT64 m_tlasResultSize = 0;

        std::vector<MeshInstanceInfo> m_tlasMeshData;
        std::shared_ptr<Buffer> m_meshIdBuffer;

        bool m_raytracingSupported = false;
    };
} // namespace Wild
