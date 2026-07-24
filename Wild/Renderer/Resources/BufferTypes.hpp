#pragma once

#include <cstdint>
#include <string>

namespace Wild
{
    enum class BufferUsage : uint32_t
    {
        None = 0,
        Vertex = 1 << 0,
        Index = 1 << 1,
        Constant = 1 << 2,    // 256 bytes aligned
        ShaderRead = 1 << 3,  // SRV
        ShaderWrite = 1 << 4, // UAV
        Indirect = 1 << 5,    // ExecuteIndirect args
        CopySrc = 1 << 6,
        CopyDst = 1 << 7,
        AccelerationStruct = 1 << 8, // TLAS/BLAS
    };

    // Bitwise operators so BufferUsage can be combined and tested like a flag set
    constexpr BufferUsage operator|(BufferUsage a, BufferUsage b)
    {
        return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    constexpr BufferUsage operator&(BufferUsage a, BufferUsage b)
    {
        return static_cast<BufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }

    inline bool HasFlag(BufferUsage value, BufferUsage flag)
    {
        return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
    }

    enum class MemoryAccess
    {
        GpuOnly,  // Default heap
        CpuToGpu, // Upload heap
        GpuToCpu, // Readback heap
    };

    struct BufferDesc
    {
        uint64_t size = 0;
        uint32_t stride = 0;
        BufferUsage usage = BufferUsage::None;
        MemoryAccess access = MemoryAccess::GpuOnly;
        std::string name = "default";
    };
} // namespace Wild
