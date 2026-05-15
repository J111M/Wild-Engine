#include "Renderer/AccelerationStructure.hpp"

namespace Wild
{
    AccelerationStructureManager::AccelerationStructureManager()
    {
        if (engine.GetGfxContext()->GetCapabilities().SupportsRayTracing()) { m_raytracingSupported = true; }
    }

    uint32_t AccelerationStructureManager::AddMeshInfo(const MeshInstanceInfo& infoDesc)
    {
        if (m_raytracingSupported)
        {
            m_tlasMeshData.emplace_back(infoDesc);

            return static_cast<uint32_t>(m_tlasMeshData.size() - 1);
        }
    }

    uint32_t AccelerationStructureManager::AddTopLevelAS(uint32_t blasIndex, const glm::mat4& transform, uint32_t instanceID,
                                                         uint32_t hitGroupIndex, uint8_t mask)
    {
        if (m_raytracingSupported)
        {
            m_dynamicTlasInstances.push_back({blasIndex, transform, instanceID, hitGroupIndex, mask});
            m_markDirty = true;
            return static_cast<uint32_t>(m_dynamicTlasInstances.size() - 1);
        }
    }

    // TODO add support for different geometry types than opaque
    uint32_t AccelerationStructureManager::AddBottomLevelAS(D3D12_RAYTRACING_GEOMETRY_DESC* geomDescs, uint32_t geomCount,
                                                            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags)
    {
        if (m_raytracingSupported)
        {
            BLASEntry entry{};

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.NumDescs = geomCount;
            inputs.pGeometryDescs = geomDescs;
            inputs.Flags = flags;

            auto gfxContext = engine.GetGfxContext();

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
            gfxContext->GetDevice7()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);

            BufferDesc scratchDesc{};
            scratchDesc.bufferSize = prebuild.ScratchDataSizeInBytes;
            scratchDesc.numOfElements = 1;
            scratchDesc.state = D3D12_RESOURCE_STATE_COMMON;
            entry.scratch = std::make_unique<Buffer>(scratchDesc, BufferType::uav);

            BufferDesc resultDesc{};
            resultDesc.bufferSize = prebuild.ResultDataMaxSizeInBytes;
            resultDesc.numOfElements = 1;
            resultDesc.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            entry.result = std::make_unique<Buffer>(resultDesc, BufferType::uav);

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
            buildDesc.Inputs = inputs;
            buildDesc.ScratchAccelerationStructureData = entry.scratch->GetBuffer()->GetGPUVirtualAddress();
            buildDesc.DestAccelerationStructureData = entry.result->GetBuffer()->GetGPUVirtualAddress();

            auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

            list.GetList4()->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = entry.result->GetBuffer();
            list.GetList()->ResourceBarrier(1, &barrier);

            // Execute the command list
            list.Close();
            gfxContext->GetCommandQueue(QueueType::Direct)->ExecuteList(list);
            gfxContext->GetCommandQueue(QueueType::Direct)->WaitForFence();

            uint32_t index = static_cast<uint32_t>(m_blasEntries.size());
            m_blasEntries.push_back(std::move(entry));

            // Return index for tlas instance
            return index;
        }
    }

    void AccelerationStructureManager::UpdateTransform(uint32_t instanceIndex, const glm::mat4& transform)
    {
        if (m_raytracingSupported)
        {
            m_dynamicTlasInstances[instanceIndex].transform = transform;
            m_markDirty = true;
        }
    }

    void AccelerationStructureManager::RemoveInstance(uint32_t instanceIndex)
    {
        if (m_raytracingSupported)
        {
            m_dynamicTlasInstances.erase(m_dynamicTlasInstances.begin() + instanceIndex);
            m_markDirty = true;
        }
    }

    void AccelerationStructureManager::UpdateTLAS()
    {
        if (m_raytracingSupported)
        {
            if (!m_markDirty && m_tlasResult) { return; }
            if (m_dynamicTlasInstances.empty())
            {
                WD_WARN("TLAS instances are empty can't update.");
                return;
            }

            // Instance description
            std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs(m_dynamicTlasInstances.size());

            for (size_t i = 0; i < m_dynamicTlasInstances.size(); i++)
            {
                const auto& inst = m_dynamicTlasInstances[i];
                auto& desc = instanceDescs[i];
                memset(&desc, 0, sizeof(desc));

                // Dx12 expects 3x4 row major so convert it over
                glm::mat4 t = glm::transpose(inst.transform);
                memcpy(desc.Transform, &t, sizeof(desc.Transform));

                desc.InstanceID = inst.instanceID;
                desc.InstanceMask = inst.mask;
                desc.InstanceContributionToHitGroupIndex = inst.hitGroupIndex;
                desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
                desc.AccelerationStructure = m_blasEntries[inst.blasIndex].result->GetBuffer()->GetGPUVirtualAddress();
            }

            // Upload instance descs
            BufferDesc instanceBuffDesc{};
            instanceBuffDesc.bufferSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
            instanceBuffDesc.numOfElements = instanceDescs.size();

            // Upload buffer
            m_instanceDescsBuffer = std::make_unique<Buffer>(instanceBuffDesc, BufferType::uav);
            m_instanceDescsBuffer->UploadToGPU(instanceDescs.data());

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.NumDescs = static_cast<uint32_t>(m_dynamicTlasInstances.size());
            inputs.InstanceDescs = m_instanceDescsBuffer->GetBuffer()->GetGPUVirtualAddress();
            inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
            engine.GetGfxContext()->GetDevice7()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);

            // Check if tlas has new entries and needs to be rebuild
            bool needsFullRebuild = !m_tlasResult || m_tlasResultSize < prebuild.ResultDataMaxSizeInBytes;

            if (needsFullRebuild)
            {
                m_tlasScratch.reset();
                m_tlasResult.reset();
                m_tlasUpdateScratch.reset();

                BufferDesc scratchDesc{};
                scratchDesc.bufferSize = prebuild.ScratchDataSizeInBytes;
                scratchDesc.numOfElements = 1;
                scratchDesc.state = D3D12_RESOURCE_STATE_COMMON;
                m_tlasScratch = std::make_unique<Buffer>(scratchDesc, BufferType::uav);

                BufferDesc resultDesc{};
                resultDesc.bufferSize = prebuild.ResultDataMaxSizeInBytes;
                resultDesc.numOfElements = 1;
                resultDesc.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
                m_tlasResult = std::make_unique<Buffer>(resultDesc, BufferType::uav);
                m_tlasResultSize = prebuild.ResultDataMaxSizeInBytes;

                // Update scratch
                BufferDesc updateScratchDesc{};
                updateScratchDesc.bufferSize = prebuild.UpdateScratchDataSizeInBytes;
                updateScratchDesc.numOfElements = 1;
                updateScratchDesc.state = D3D12_RESOURCE_STATE_COMMON;
                m_tlasUpdateScratch = std::make_unique<Buffer>(updateScratchDesc, BufferType::uav);

                // Update mesh instance buffer
                BufferDesc desc{};
                desc.bufferSize = sizeof(MeshInstanceInfo);
                desc.numOfElements = m_initialCapacity;
                m_meshIdBuffer = std::make_unique<Buffer>(desc, uav);
                m_meshIdBuffer->UploadToGPU(m_tlasMeshData.data(), m_tlasMeshData.size() * sizeof(MeshInstanceInfo));
            }

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
            buildDesc.Inputs = inputs;
            buildDesc.DestAccelerationStructureData = m_tlasResult->GetBuffer()->GetGPUVirtualAddress();

            if (!needsFullRebuild && m_tlasIsBuild)
            {
                buildDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
                buildDesc.SourceAccelerationStructureData = m_tlasResult->GetBuffer()->GetGPUVirtualAddress();
                buildDesc.ScratchAccelerationStructureData = m_tlasUpdateScratch->GetBuffer()->GetGPUVirtualAddress();
            }
            else
            {
                buildDesc.ScratchAccelerationStructureData = m_tlasScratch->GetBuffer()->GetGPUVirtualAddress();
            }

            auto cmd = engine.GetGfxContext()->GetCommandList();

            cmd->GetList4()->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = m_tlasResult->GetBuffer();
            cmd->GetList()->ResourceBarrier(1, &barrier);

            m_markDirty = false;
            m_tlasIsBuild = true;
        }
    }
} // namespace Wild
