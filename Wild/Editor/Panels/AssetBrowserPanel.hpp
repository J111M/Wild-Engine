#pragma once

#include "Editor/EditorPanel.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Wild
{
    class Texture;

    class AssetBrowserPanel final : public EditorPanel
    {
      public:
        AssetBrowserPanel();

        const char* Name() const override { return "Assets"; }
        const char* Category() const override { return "Core"; }
        void OnRender() override;

        void AddDroppedFiles(const char** paths, int count);

      private:
        void DrawBreadcrumbs();
        void DrawFileGrid();
        void DrawPendingImports();

        std::shared_ptr<Texture> GetThumbnail(const std::filesystem::path& path);

        std::filesystem::path m_root;
        std::filesystem::path m_current;
        std::vector<std::string> m_pendingImports;

        std::unordered_map<std::string, std::shared_ptr<Texture>> m_thumbnails;
        int m_thumbnailLoadsThisFrame = 0;
    };
} // namespace Wild
