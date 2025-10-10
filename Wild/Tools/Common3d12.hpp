#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#include <filesystem>

inline std::wstring StringToWString(const std::string& str) { return std::filesystem::path(str).wstring(); }

inline std::string WStringToString(const std::wstring& str) { return std::filesystem::path(str).string(); }