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

    constexpr ShaderCacheSupport ConvertShaderCacheSupport(D3D12_SHADER_CACHE_SUPPORT_FLAGS flags)
    {
        if (flags == D3D12_SHADER_CACHE_SUPPORT_NONE) return ShaderCacheSupport::NotSupported;

        if (flags & D3D12_SHADER_CACHE_SUPPORT_DRIVER_MANAGED_CACHE) return ShaderCacheSupport::DriverManaged;

        if (flags & D3D12_SHADER_CACHE_SUPPORT_LIBRARY) return ShaderCacheSupport::Library;

        if (flags & D3D12_SHADER_CACHE_SUPPORT_SINGLE_PSO) return ShaderCacheSupport::SinglePSO;

        return ShaderCacheSupport::NotSupported;
    }

    bool GfxCapabilities::GetSupportedFeatures(GfxContext* context)
    {
        CD3DX12FeatureSupport featureSupport;
        featureSupport.Init(context->GetDevice().Get());

        m_meshShaderSupported = ConvertMeshShaderTier(featureSupport.MeshShaderTier());
        m_shaderCacheSupported = ConvertShaderCacheSupport(featureSupport.ShaderCacheSupportFlags());

        return true;
    }
} // namespace Wild
