#pragma once

// Glyphs from the Segoe MDL2 Assets / Segoe Fluent Icons system font, merged
// into the editor font atlas by LoadEditorFont. Icons are regular text and
// live in the same atlas texture as the letters, so they render identically
// in docked and floating windows. No image files involved.
//
// Codepoint reference:
// https://learn.microsoft.com/en-us/windows/apps/design/style/segoe-ui-symbol-font

#define WD_ICON_MOVE "\xEE\x9F\x82"   // U+E7C2 Move
#define WD_ICON_ROTATE "\xEE\x9E\xAD" // U+E7AD Rotate
#define WD_ICON_SCALE "\xEE\x9D\x80"  // U+E740 FullScreen (expand arrows)
#define WD_ICON_FOLDER "\xEE\xA2\xB7" // U+E8B7 Folder
#define WD_ICON_FILE "\xEE\xA2\xA5"   // U+E8A5 Document
#define WD_ICON_IMAGE "\xEE\xA2\xB9"  // U+E8B9 Picture
#define WD_ICON_MODEL "\xEE\x9E\xB8"  // U+E7B8 Package (3D model files)

namespace Wild
{
    bool EditorIconsLoaded();
} // namespace Wild
