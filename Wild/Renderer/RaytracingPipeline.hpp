#pragma once

#include "Renderer/Resources/Buffer.hpp"

namespace Wild
{
    class RaytracingPipeline
    {
      public:
        RaytracingPipeline(const PipelineStateSettings& settings, ComPtr<ID3D12RootSignature> rootSignature);
        ~RaytracingPipeline() {};

        ComPtr<ID3D12StateObject> GetStateObject() const { return m_stateObject; }

        D3D12_DISPATCH_RAYS_DESC GetDispatchDesc() const { return m_dispatchDesc; }

      private:
        void InitializeSBT(const PipelineStateSettings& settings, ComPtr<ID3D12RootSignature> rootSignature);

        ComPtr<ID3D12StateObject> m_stateObject{};
        ComPtr<ID3D12StateObjectProperties> m_stateObjectProperties{};

        D3D12_DISPATCH_RAYS_DESC m_dispatchDesc{};

        std::unique_ptr<Buffer> m_rayGenSBT{};
        std::unique_ptr<Buffer> m_missSBT{};
        std::unique_ptr<Buffer> m_hitGroupSBT{};
    };
} // namespace Wild
