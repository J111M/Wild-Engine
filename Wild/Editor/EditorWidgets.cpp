#include "Editor/EditorWidgets.hpp"

#include "Renderer/Resources/Texture.hpp"

#include <imgui.h>

namespace Wild
{
    namespace
    {
        bool TintedDragFloat(const char* id, float& value, float speed, const ImVec4& borderColor, const ImVec4& bgColor)
        {
            ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, bgColor);
            bool changed = ImGui::DragFloat(id, &value, speed);
            ImGui::PopStyleColor(2);
            return changed;
        }
    } // namespace

    bool EditorWidgets::DragFloat3Colored(const char* name, glm::vec3& value, float speed)
    {
        ImGui::PushID(name);
        ImGui::BeginGroup();
        ImGui::PushItemWidth((ImGui::CalcItemWidth() - 6.0f) / 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        bool changed = false;

        changed |= TintedDragFloat("##x", value.x, speed, ImVec4(0.75f, 0.20f, 0.20f, 1.0f), ImVec4(0.20f, 0.05f, 0.05f, 1.0f));
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        changed |= TintedDragFloat("##y", value.y, speed, ImVec4(0.20f, 0.65f, 0.20f, 1.0f), ImVec4(0.05f, 0.20f, 0.05f, 1.0f));
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        changed |= TintedDragFloat("##z", value.z, speed, ImVec4(0.20f, 0.35f, 0.80f, 1.0f), ImVec4(0.05f, 0.05f, 0.20f, 1.0f));

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(name);

        ImGui::PopStyleVar();
        ImGui::PopItemWidth();
        ImGui::EndGroup();
        ImGui::PopID();
        return changed;
    }

    void EditorWidgets::TextureImage(Texture* texture, glm::vec2 imageSize)
    {
        if (texture) ImGui::Image((ImTextureID)texture->GetSrv()->GetGpuHandle().ptr, ImVec2(imageSize.x, imageSize.y));
    }

    void EditorWidgets::TextureImage(const std::shared_ptr<Texture>& texture, glm::vec2 imageSize)
    {
        TextureImage(texture.get(), imageSize);
    }
} // namespace Wild
