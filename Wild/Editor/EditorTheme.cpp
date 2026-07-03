#include "Editor/EditorTheme.hpp"

#include "Editor/EditorIcons.hpp"

#include <imgui.h>

#include <cstdlib>
#include <filesystem>

namespace Wild
{
    void ApplyEditorTheme()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style = ImGuiStyle();

        style.WindowRounding = 4.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.PopupRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 3.0f;
        style.ScrollbarRounding = 6.0f;

        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.FramePadding = ImVec2(8.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 5.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.IndentSpacing = 18.0f;
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 10.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.TabBarBorderSize = 1.0f;

        style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
        style.SeparatorTextBorderSize = 2.0f;

        const ImVec4 bg = ImVec4(0.118f, 0.118f, 0.133f, 1.00f);
        const ImVec4 bgDark = ImVec4(0.098f, 0.098f, 0.110f, 1.00f);
        const ImVec4 bgLight = ImVec4(0.160f, 0.160f, 0.180f, 1.00f);
        const ImVec4 bgLighter = ImVec4(0.205f, 0.205f, 0.228f, 1.00f);
        const ImVec4 border = ImVec4(0.240f, 0.240f, 0.265f, 0.60f);
        const ImVec4 text = ImVec4(0.860f, 0.860f, 0.880f, 1.00f);
        const ImVec4 textDim = ImVec4(0.500f, 0.500f, 0.540f, 1.00f);
        const ImVec4 accent = ImVec4(0.180f, 0.430f, 0.470f, 1.00f);
        const ImVec4 accentHover = ImVec4(0.230f, 0.520f, 0.560f, 1.00f);
        const ImVec4 accentActive = ImVec4(0.280f, 0.600f, 0.640f, 1.00f);
        const ImVec4 accentDim = ImVec4(0.180f, 0.430f, 0.470f, 0.45f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = text;
        colors[ImGuiCol_TextDisabled] = textDim;
        colors[ImGuiCol_WindowBg] = bg;
        colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_PopupBg] = bgDark;
        colors[ImGuiCol_Border] = border;
        colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_FrameBg] = bgLight;
        colors[ImGuiCol_FrameBgHovered] = bgLighter;
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.240f, 0.240f, 0.270f, 1.00f);
        colors[ImGuiCol_TitleBg] = bgDark;
        colors[ImGuiCol_TitleBgActive] = bgDark;
        colors[ImGuiCol_TitleBgCollapsed] = bgDark;
        colors[ImGuiCol_MenuBarBg] = bgDark;
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_ScrollbarGrab] = bgLighter;
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.28f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = accent;
        colors[ImGuiCol_CheckMark] = accentActive;
        colors[ImGuiCol_SliderGrab] = accent;
        colors[ImGuiCol_SliderGrabActive] = accentActive;
        colors[ImGuiCol_Button] = bgLight;
        colors[ImGuiCol_ButtonHovered] = accentHover;
        colors[ImGuiCol_ButtonActive] = accentActive;
        colors[ImGuiCol_Header] = accentDim;
        colors[ImGuiCol_HeaderHovered] = accentHover;
        colors[ImGuiCol_HeaderActive] = accentActive;
        colors[ImGuiCol_Separator] = border;
        colors[ImGuiCol_SeparatorHovered] = accentHover;
        colors[ImGuiCol_SeparatorActive] = accentActive;
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_ResizeGripHovered] = accentHover;
        colors[ImGuiCol_ResizeGripActive] = accentActive;
        colors[ImGuiCol_Tab] = bgDark;
        colors[ImGuiCol_TabHovered] = accentHover;
        colors[ImGuiCol_TabSelected] = bg;
        colors[ImGuiCol_TabSelectedOverline] = accentActive;
        colors[ImGuiCol_TabDimmed] = bgDark;
        colors[ImGuiCol_TabDimmedSelected] = bg;
        colors[ImGuiCol_TabDimmedSelectedOverline] = accentDim;
        colors[ImGuiCol_DockingPreview] = accentDim;
        colors[ImGuiCol_DockingEmptyBg] = bgDark;
        colors[ImGuiCol_PlotLines] = accentActive;
        colors[ImGuiCol_PlotLinesHovered] = accentHover;
        colors[ImGuiCol_PlotHistogram] = accent;
        colors[ImGuiCol_PlotHistogramHovered] = accentHover;
        colors[ImGuiCol_TableHeaderBg] = bgDark;
        colors[ImGuiCol_TableBorderStrong] = border;
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.20f, 0.20f, 0.22f, 0.40f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);
        colors[ImGuiCol_TextSelectedBg] = accentDim;
        colors[ImGuiCol_DragDropTarget] = accentActive;
        colors[ImGuiCol_NavCursor] = accentActive;
        colors[ImGuiCol_NavWindowingHighlight] = accentActive;
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.4f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            colors[ImGuiCol_WindowBg].w = 1.0f;
        }
    }

    namespace
    {
        bool s_editorIconsLoaded = false;
    }

    bool EditorIconsLoaded() { return s_editorIconsLoaded; }

    void LoadEditorFont(const char* ttfPath)
    {
        ImGuiIO& io = ImGui::GetIO();

        bool customFont = false;
        if (ttfPath && std::filesystem::exists(ttfPath)) { customFont = io.Fonts->AddFontFromFileTTF(ttfPath, 16.0f) != nullptr; }
        if (!customFont) io.Fonts->AddFontDefault();

        const float baseFontSize = customFont ? 16.0f : 13.0f;

        const char* winDir = std::getenv("WINDIR");
        std::filesystem::path fontDir = std::filesystem::path(winDir ? winDir : "C:/Windows") / "Fonts";

        for (const char* iconFont : {"segmdl2.ttf", "SegoeIcons.ttf"})
        {
            std::filesystem::path iconPath = fontDir / iconFont;
            if (!std::filesystem::exists(iconPath)) continue;

            ImFontConfig config{};

            // add the glyphs to the font above instead of making a new font
            config.MergeMode = true;

            config.GlyphOffset.y = baseFontSize * 0.2f;

            if (io.Fonts->AddFontFromFileTTF(iconPath.string().c_str(), baseFontSize, &config))
            {
                s_editorIconsLoaded = true;
                break;
            }
        }
    }
} // namespace Wild
