#pragma once

namespace Wild
{
    enum class MeshShaderSupport : uint8_t
    {
        TierNotSupported,
        Tier1
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

      protected:
        MeshShaderSupport m_meshShaderSupported = MeshShaderSupport::TierNotSupported;
    };
} // namespace Wild
