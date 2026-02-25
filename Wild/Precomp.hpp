#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
//#define GLM_FORCE_RIGHT_HANDED

#define IMGUI_DEFINE_MATH_OPERATORS

#include "Tools/Log.hpp"

#ifdef DEBUG
#define IFCHECK(condition, ...)                                                                                                  \
    if (!(condition)) WD_ERROR(__VA_ARGS__)
#else
#define IFCHECK(...)                                                                                                             \
    {                                                                                                                            \
    }
#endif

#include <comdef.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max

#include <stdexcept>
#include <string>
#include <wrl.h>

using namespace Microsoft::WRL;

inline void ThrowIfFailed(HRESULT hr, const char* msg = nullptr)
{
    if (FAILED(hr))
    {
        char hrMsg[512] = {};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr,
                       hr,
                       MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                       hrMsg,
                       sizeof(hrMsg),
                       nullptr);
        size_t len = strlen(hrMsg);
        if (len > 0 && hrMsg[len - 1] == '\n') hrMsg[len - 1] = '\0';
        if (len > 1 && hrMsg[len - 2] == '\r') hrMsg[len - 2] = '\0';

        char buffer[1024];
        sprintf_s(buffer, "HRESULT 0x%08X: %s", static_cast<unsigned int>(hr), hrMsg);

        std::string errMsg = buffer;
        if (msg) errMsg = std::string(msg) + " | " + errMsg;

        WD_FATAL(errMsg);
        throw std::runtime_error(errMsg);
    }
}

class NonCopyable
{
  protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = delete;
    NonCopyable& operator=(NonCopyable&&) = delete;
};

// TODO only include into engine files
#include "Core/Engine.hpp"
