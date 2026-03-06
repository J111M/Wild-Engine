#pragma once

namespace Wild
{
    enum class MeshShaderSupport : uint8_t
    {
        TierNotSupported,
        Tier1
    };

    enum class ShaderCacheSupport : uint8_t
    {
        NotSupported,
        SinglePSO,
        DriverManaged,
        Library
    };

    class GfxContext;

    class GfxCapabilities
    {
      public:
        GfxCapabilities() = default;
        ~GfxCapabilities() = default;

        bool GetSupportedFeatures(GfxContext* context);

        bool SupportsMeshShaders() const { return CheckMeshShaderSupport(MeshShaderSupport::Tier1); }
        bool CheckMeshShaderSupport(MeshShaderSupport tier) const { return m_meshShaderSupported >= tier; }

        bool SupportsShaderCache() const { return CheckMeshShaderSupport(MeshShaderSupport::Tier1); }
        bool CheckLibraryPSOCacheSupport() const { return m_shaderCacheSupported == ShaderCacheSupport::Library; }
        bool CheckDriverManagedPSOCacheSupport() const { return m_shaderCacheSupported == ShaderCacheSupport::DriverManaged; }

      protected:
        MeshShaderSupport m_meshShaderSupported = MeshShaderSupport::TierNotSupported;
        ShaderCacheSupport m_shaderCacheSupported = ShaderCacheSupport::NotSupported;
    };
} // namespace Wild
