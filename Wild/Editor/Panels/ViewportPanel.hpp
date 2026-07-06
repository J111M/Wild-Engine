#pragma once

#include "Editor/EditorPanel.hpp"
#include "Editor/EditorState.hpp"

namespace Wild
{
    class ViewportPanel final : public EditorPanel
    {
      public:
        explicit ViewportPanel(EditorState& state) : m_state(state) {}

        const char* Name() const override { return "Viewport"; }
        const char* Category() const override { return "Core"; }
        void OnRender() override;

        ImGuiWindowFlags WindowFlags() const override
        { return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse; }
        bool NoPadding() const override { return true; }

        void SetTexture(ImTextureID texture) { m_texture = texture; }

      private:
        void DrawGizmoToolbar();

        void SpawnDroppedModel(const char* assetPath);
        void TryPickEntity();

        EditorState& m_state;
        ImTextureID m_texture{};
    };
} // namespace Wild
