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

    constexpr RayTracingSupport ConvertRayTracingTier(D3D12_RAYTRACING_TIER tier)
    {
        switch (tier)
        {
        case D3D12_RAYTRACING_TIER_NOT_SUPPORTED:
            return RayTracingSupport::TierNotSupported;
        case D3D12_RAYTRACING_TIER_1_0:
            return RayTracingSupport::Tier10;
        case D3D12_RAYTRACING_TIER_1_1:
            return RayTracingSupport::Tier11;
        case D3D12_RAYTRACING_TIER_1_2:
            return RayTracingSupport::Tier12;
        }
        return RayTracingSupport::TierNotSupported;
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

        // Querry feature support from device
        m_meshShaderSupported = ConvertMeshShaderTier(featureSupport.MeshShaderTier());
        m_shaderCacheSupported = ConvertShaderCacheSupport(featureSupport.ShaderCacheSupportFlags());
        m_rayTracingSupport = ConvertRayTracingTier(featureSupport.RaytracingTier());

        return true;
    }
} // namespace Wild
