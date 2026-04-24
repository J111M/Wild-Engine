#include "Renderer/RaytracingPipeline.hpp"

namespace Wild
{
    RaytracingPipeline::RaytracingPipeline(const PipelineStateSettings& settings, ComPtr<ID3D12RootSignature> rootSignature)
    {
        InitializeSBT(settings, rootSignature);
    }

    void RaytracingPipeline::InitializeSBT(const PipelineStateSettings& settings, ComPtr<ID3D12RootSignature> rootSignature)
    {
        auto device = engine.GetGfxContext()->GetDevice7();

        if (!settings.ShaderState.rayTracingShader)
        {
            WD_ERROR("Pipeline state {} doesn't contain a raytracing shader", settings.PipelineName);
            return;
        }

        auto shader = settings.ShaderState.rayTracingShader;

        CD3DX12_STATE_OBJECT_DESC raytracingPipelineDesc{D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE};

        auto entryPoints = shader->GetRTEntryPoints();
        if (!entryPoints) WD_WARN("Raytrace pipeline doesn't have a valid shader");

        for (auto& ep : entryPoints->shaderBlobs)
        {
            auto library = raytracingPipelineDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
            D3D12_SHADER_BYTECODE bytecode;
            bytecode.pShaderBytecode = ep.blob->getBufferPointer();
            bytecode.BytecodeLength = ep.blob->getBufferSize();
            library->SetDXILLibrary(&bytecode);
        }

        // Pair hit groups, 1 hit group per cHit shader
        std::vector<std::wstring> hitGroupNames;
        for (size_t i = 0; i < entryPoints->closestHit.size(); i++)
        {
            std::wstring groupName = L"HitGroup_" + std::to_wstring(i);
            hitGroupNames.push_back(groupName);

            auto hitGroup = raytracingPipelineDesc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
            hitGroup->SetClosestHitShaderImport(entryPoints->closestHit[i].c_str());
            if (i < entryPoints->anyHit.size()) hitGroup->SetAnyHitShaderImport(entryPoints->anyHit[i].c_str());
            if (i < entryPoints->intersection.size())
            {
                hitGroup->SetIntersectionShaderImport(entryPoints->intersection[i].c_str());
                hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);
            }
            else
            {
                hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
            }
            hitGroup->SetHitGroupExport(groupName.c_str());
        }

        auto shaderConfig = raytracingPipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
        shaderConfig->Config(settings.raytracingState.payloadSize, settings.raytracingState.attributeSize);

        auto globalRootSig = raytracingPipelineDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        globalRootSig->SetRootSignature(rootSignature.Get());

        auto pipelineConfig = raytracingPipelineDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
        pipelineConfig->Config(settings.raytracingState.rayRecursionDepth);

        device->CreateStateObject(raytracingPipelineDesc, IID_PPV_ARGS(&m_stateObject));

        // Setting up shader binding table
        ComPtr<ID3D12StateObjectProperties> props;

        // Querry for property interface
        m_stateObject.As(&props);

        constexpr UINT shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        constexpr UINT sbtAlignment = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
        constexpr UINT sbtEntrySize = (shaderIdSize + sbtAlignment - 1) & ~(sbtAlignment - 1);

        // Create ray gen sbt buffer
        void* rayGenId = props->GetShaderIdentifier(entryPoints->rayGen[0].c_str());
        BufferDesc rayGenDesc{};
        rayGenDesc.bufferSize = sbtEntrySize;
        m_rayGenSBT = std::make_unique<Buffer>(rayGenDesc, BufferType::shaderBindingTable);
        m_rayGenSBT->WriteData(rayGenId);

        // Miss table
        std::vector<uint8_t> missData(sbtEntrySize * entryPoints->miss.size());
        for (size_t i = 0; i < entryPoints->miss.size(); i++)
        {
            void* id = props->GetShaderIdentifier(entryPoints->miss[i].c_str());
            memcpy(missData.data() + i * sbtEntrySize, id, shaderIdSize);
        }

        // Create miss sbt buffer
        BufferDesc missDesc{};
        missDesc.bufferSize = missData.size();
        m_missSBT = std::make_unique<Buffer>(missDesc, BufferType::shaderBindingTable);
        m_missSBT->WriteData(missData.data());

        // Combined hit group table
        std::vector<uint8_t> hitGroupData(sbtEntrySize * hitGroupNames.size());
        for (size_t i = 0; i < hitGroupNames.size(); i++)
        {
            void* id = props->GetShaderIdentifier(hitGroupNames[i].c_str());
            memcpy(hitGroupData.data() + i * sbtEntrySize, id, shaderIdSize);
        }

        BufferDesc hitGroupDesc{};
        hitGroupDesc.bufferSize = hitGroupData.size();
        m_hitGroupSBT = std::make_unique<Buffer>(hitGroupDesc, BufferType::shaderBindingTable);
        m_hitGroupSBT->WriteData(hitGroupData.data());

        // Setup the dispatch description
        m_dispatchDesc.RayGenerationShaderRecord.StartAddress = m_rayGenSBT->GetBuffer()->GetGPUVirtualAddress();
        m_dispatchDesc.RayGenerationShaderRecord.SizeInBytes = sbtEntrySize;

        m_dispatchDesc.MissShaderTable.StartAddress = m_missSBT->GetBuffer()->GetGPUVirtualAddress();
        m_dispatchDesc.MissShaderTable.SizeInBytes = sbtEntrySize * static_cast<UINT>(entryPoints->miss.size());
        m_dispatchDesc.MissShaderTable.StrideInBytes = sbtEntrySize;

        m_dispatchDesc.HitGroupTable.StartAddress = m_hitGroupSBT->GetBuffer()->GetGPUVirtualAddress();
        m_dispatchDesc.HitGroupTable.SizeInBytes = sbtEntrySize * (UINT)hitGroupNames.size();
        m_dispatchDesc.HitGroupTable.StrideInBytes = sbtEntrySize;
    }
} // namespace Wild
