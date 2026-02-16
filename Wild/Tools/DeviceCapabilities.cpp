#include "Tools/DeviceCapabilities.hpp"
#include "Tools/Common3d12.hpp"

namespace Wild
{
    constexpr MeshShaderSupport ConvertMeshShaderTier(D3D12_MESH_SHADER_TIER tier)
    {
        switch (tier)
        {
        case D3D12_MESH_SHADER_TIER_NOT_SUPPORTED:
            return MeshShaderSupport::TierNotSupported;
        case D3D12_MESH_SHADER_TIER_1:
            return MeshShaderSupport::Tier1;
        }
        return MeshShaderSupport::TierNotSupported;
    }

    bool GfxCapabilities::GetSupportedFeatures(GfxContext* context)
    {
        CD3DX12FeatureSupport featureSupport;
        featureSupport.Init(context->GetDevice().Get());

        m_meshShaderSupported = ConvertMeshShaderTier(featureSupport.MeshShaderTier());

        return true;
    }
} // namespace Wild
