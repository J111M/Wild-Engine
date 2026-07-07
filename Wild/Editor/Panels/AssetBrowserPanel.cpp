#include "Editor/Panels/AssetBrowserPanel.hpp"

#include "Core/Engine.hpp"
#include "Editor/EditorIcons.hpp"
#include "Renderer/Resources/Texture.hpp"
#include "Tools/Log.hpp"

#include <imgui.h>

#include <algorithm>
#include <system_error>

namespace Wild
{
    namespace
    {
        // Formats stb_image can load
        bool IsImageFile(const std::filesystem::path& path)
        {
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
        }

        bool IsModelFile(const std::filesystem::path& path)
        {
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            return ext == ".gltf" || ext == ".glb";
        }

        bool IsPrefabFile(const std::filesystem::path& path)
        {
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            return ext == ".prefab";
        }
    } // namespace

    AssetBrowserPanel::AssetBrowserPanel()
    {
        std::error_code ec;
#ifdef ASSETS_SOURCE_DIR
        if (std::filesystem::exists(ASSETS_SOURCE_DIR, ec)) m_root = ASSETS_SOURCE_DIR;
        else
#endif
            m_root = std::filesystem::exists("Assets", ec) ? std::filesystem::path("Assets") : std::filesystem::current_path(ec);
        m_current = m_root;
    }

    void AssetBrowserPanel::OnRender()
    {
        m_thumbnailLoadsThisFrame = 0;

        DrawBreadcrumbs();
        ImGui::Separator();
        DrawPendingImports();
        DrawFileGrid();
    }

    std::shared_ptr<Texture> AssetBrowserPanel::GetThumbnail(const std::filesystem::path& path)
    {
        std::string key = path.generic_string();

        if (auto it = m_thumbnails.find(key); it != m_thumbnails.end()) return it->second;

        // One upload per frame keeps folder browsing from hitching
        if (m_thumbnailLoadsThisFrame >= 1) return nullptr;
        m_thumbnailLoadsThisFrame++;

        auto texture = engine.GetResourceSystems().m_textureResourceSystem->GetOrCreateResource(key, key, TextureType::TEXTURE_2D);
        m_thumbnails[key] = texture;
        return texture;
    }

    void AssetBrowserPanel::AddDroppedFiles(const char** paths, int count)
    {
        for (int i = 0; i < count; i++)
        {
            m_pendingImports.emplace_back(paths[i]);
            WD_INFO("Dropped file: {}", paths[i]);
        }
    }

    void AssetBrowserPanel::DrawBreadcrumbs()
    {
        if (ImGui::Button("Assets")) m_current = m_root;

        // One clickable crumb per directory below the root
        std::filesystem::path rebuilt = m_root;
        for (const auto& part : std::filesystem::relative(m_current, m_root))
        {
            if (part == ".") continue;
            rebuilt /= part;
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::TextDisabled("/");
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::PushID(rebuilt.string().c_str());
            if (ImGui::SmallButton(part.string().c_str())) m_current = rebuilt;
            ImGui::PopID();
        }
    }

    void AssetBrowserPanel::DrawPendingImports()
    {
        if (m_pendingImports.empty()) return;

        if (ImGui::CollapsingHeader("Pending Imports", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (const auto& path : m_pendingImports)
                ImGui::BulletText("%s", path.c_str());

            if (ImGui::Button("Import All"))
            {
                // TODO: import — route these into the resource systems
                WD_WARN("Asset import is not implemented yet ({} file(s) pending)", m_pendingImports.size());
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard")) m_pendingImports.clear();
            ImGui::Separator();
        }
    }

    void AssetBrowserPanel::DrawFileGrid()
    {
        std::error_code ec;
        if (!std::filesystem::exists(m_current, ec))
        {
            ImGui::TextDisabled("Folder not found: %s", m_current.string().c_str());
            return;
        }

        struct GridEntry
        {
            std::filesystem::path path;
            bool isDirectory = false;
        };

        std::vector<GridEntry> entries;
        for (const auto& entry : std::filesystem::directory_iterator(m_current, ec))
            entries.push_back({entry.path(), entry.is_directory(ec)});

        std::sort(entries.begin(), entries.end(), [](const GridEntry& a, const GridEntry& b) {
            if (a.isDirectory != b.isDirectory) return a.isDirectory;
            return a.path.filename() < b.path.filename();
        });

        const float cellSize = 96.0f;
        int columns = std::max(1, (int)(ImGui::GetContentRegionAvail().x / cellSize));

        ImGui::BeginChild("##AssetGrid");
        ImGui::Columns(columns, nullptr, false);

        const ImVec2 cellButtonSize = ImVec2(cellSize - 12.0f, 48.0f);

        for (const auto& entry : entries)
        {
            std::string filename = entry.path.filename().string();
            ImGui::PushID(filename.c_str());

            std::shared_ptr<Texture> thumbnail;
            if (!entry.isDirectory && IsImageFile(entry.path)) thumbnail = GetThumbnail(entry.path);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

            // TODO modify the thumbnail to not stretch out
            if (thumbnail && thumbnail->GetSrv())
            {
                const ImVec2 padding = ImGui::GetStyle().FramePadding;
                ImGui::ImageButton("##thumbnail",
                                   (ImTextureID)thumbnail->GetSrv()->GetGpuHandle().ptr,
                                   ImVec2(cellButtonSize.x - padding.x * 2.0f, cellButtonSize.y - padding.y * 2.0f));
            }
            else if (EditorIconsLoaded())
            {
                const char* icon = entry.isDirectory   ? WD_ICON_FOLDER
                                   : IsImageFile(entry.path) ? WD_ICON_IMAGE
                                   : IsModelFile(entry.path) ? WD_ICON_MODEL
                                   : IsPrefabFile(entry.path) ? WD_ICON_MODEL
                                                             : WD_ICON_FILE;

                ImGui::PushFont(nullptr, 30.0f);
                ImGui::Button(icon, cellButtonSize);
                ImGui::PopFont();
            }
            else
            {
                ImGui::Button(entry.isDirectory ? "[DIR]" : "[FILE]", cellButtonSize);
            }

            ImGui::PopStyleColor();

            bool isPrefab = IsPrefabFile(entry.path);
            if (!entry.isDirectory && (IsModelFile(entry.path) || isPrefab) && ImGui::BeginDragDropSource())
            {
                std::string payloadPath = entry.path.generic_string();
                const char* payloadType = isPrefab ? "WILD_PREFAB_ASSET" : "WILD_MODEL_ASSET";
                ImGui::SetDragDropPayload(payloadType, payloadPath.c_str(), payloadPath.size() + 1);

                if (EditorIconsLoaded())
                {
                    ImGui::TextUnformatted(WD_ICON_MODEL);
                    ImGui::SameLine();
                }
                ImGui::TextUnformatted(filename.c_str());

                ImGui::EndDragDropSource();
            }

            if (entry.isDirectory && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                m_current = entry.path;

            ImGui::TextWrapped("%s", filename.c_str());
            ImGui::PopID();
            ImGui::NextColumn();
        }

        ImGui::Columns(1);
        ImGui::EndChild();
    }
} // namespace Wild
