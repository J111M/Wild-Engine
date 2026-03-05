#pragma once

#include "Tools/Common3d12.hpp"
#include "Tools/States.hpp"

#include <string>

namespace Wild
{
    struct SamplerState
    {
        D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC;
        D3D12_TEXTURE_ADDRESS_MODE addressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        D3D12_STATIC_BORDER_COLOR borderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    };

    struct Uniform
    {
        uint32_t shaderRegister{};
        uint32_t registerSpace{};
        RootParams::RootResourceType type{};
        size_t resourceSize{};

        D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
        std::vector<D3D12_DESCRIPTOR_RANGE> ranges;

        // Sampler types if used
        SamplerState samplerState{};
    };

    enum class PipelineStateType : uint8_t
    {
        Graphics,
        Compute,
        MeshPipeline
    };

    class PipelineState
    {
      public:
        PipelineState(PipelineStateType Type, const PipelineStateSettings& settings, const std::vector<Uniform>& uniforms = {});
        ~PipelineState();

        ComPtr<ID3D12PipelineState> GetPso() { return m_pso; }
        ComPtr<ID3D12RootSignature> GetRootSignature() { return m_rootSignature; }
        const PipelineStateSettings& GetPipelineSettings() { return m_settings; }

        const PipelineStateType GetPassType() const { return m_type; }

        void SerializePSO();

      private:
        void CreateRootSignature(const std::vector<Uniform>& uniforms);
        void CreateGraphicsPSO();
        void CreateComputePSO();
        void CreateMeshPipelinePSO();

        PipelineStateType m_type;
        const PipelineStateSettings& m_settings;

        ComPtr<ID3D12PipelineState> m_pso;
        ComPtr<ID3D12RootSignature> m_rootSignature;

        std::string m_name;

        bool m_isComputePass = false;
        bool m_hasRootConstant = false;
    };
} // namespace Wild
