#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#define IMGUI_DEFINE_MATH_OPERATORS

#include "Tools/Log.hpp"

#ifdef DEBUG
#define IFCHECK(condition, ...) \
    if(!(condition)) WD_ERROR(__VA_ARGS__);
#else
#define IFCHECK(...) \
{ \
} 
#endif

#include <comdef.h> 
#include <windows.h>
#include <stdexcept>
#include <string>
#include <wrl.h>

using namespace Microsoft::WRL;


inline void ThrowIfFailed(HRESULT hr, const char* msg = nullptr)
{
    if (FAILED(hr))
    {
        char buffer[256];
        sprintf_s(buffer, "HRESULT failed: 0x%08X", static_cast<unsigned int>(hr));

        std::string errMsg = buffer;
        if (msg) {
            errMsg = std::string(msg) + " | " + errMsg;

            WD_FATAL(errMsg);
        }
        else
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

#include "Core/Engine.hpp"

//static const UINT BACK_BUFFER_COUNT = 2;