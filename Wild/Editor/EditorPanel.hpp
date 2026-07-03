#pragma once

#include <imgui.h>

namespace Wild
{
    class EditorPanel
    {
      public:
        virtual ~EditorPanel() = default;

        virtual const char* Name() const = 0;
        virtual const char* Category() const { return "Core"; }
        virtual void OnRender() = 0;

        virtual ImGuiWindowFlags WindowFlags() const { return ImGuiWindowFlags_None; }

        virtual bool NoPadding() const { return false; }

        bool visible = true;
        bool defaultOpen = true;
    };
} // namespace Wild
