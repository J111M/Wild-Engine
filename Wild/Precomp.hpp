#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#define IMGUI_DEFINE_MATH_OPERATORS

#ifdef DEBUG
#define IFCHECK(condition, ...) \
    if(!(condition)) ::Dragos::Log::GetLogger()->error(__VA_ARGS__);
#else
#define IFCHECK(...) \
{ \
} 
#endif

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