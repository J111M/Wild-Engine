#pragma once

#include "Core/Camera.hpp"

#include "Renderer/GfxContext.hpp"
#include "Renderer/Resources/D3D12Resource.hpp"

#include "Tools/D3D12Common.hpp"

#include <cstddef>
#include <memory>

namespace Wild
{
    /// <summary>
    /// All camera data of the camera that is currently being rendered. Every pass reads from this instead of calculating
    /// its own matrices.
    /// </summary>
    struct SceneCameraData
    {
        glm::mat4 view{1.0f};
        glm::mat4 projection{1.0f};
        glm::mat4 viewProjection{1.0f};
        glm::mat4 inverseView{1.0f};
        glm::mat4 inverseProjection{1.0f};
        glm::mat4 inverseViewProjection{1.0f};

        // xyz = world space position, w = 1
        glm::vec4 position{0.0f, 0.0f, 0.0f, 1.0f};

        // x = near, y = far, z = vertical fov in radians, w = aspect ratio
        glm::vec4 nearFarFovAspect{};

        // Normalized world space planes in the order left, right, bottom, top, near, far
        BoundingFrustum frustum{};

        // Extracts data from the camera
        static SceneCameraData FromCamera(const Camera& camera);
    };

    /// <summary>
    /// Owns the GPU side of the scene camera data. A single persistently mapped upload buffer holds a 256 byte aligned slot
    /// per frame in flight, so writing the next frame never overwrites data the GPU can still be reading.
    /// </summary>
    class SceneDataBuffer : private NonCopyable
    {
      public:
        SceneDataBuffer();
        ~SceneDataBuffer();

        // Copies the data into the slot of this frame and makes it the active slot.
        // Returns false and leaves the active slot untouched when the frame index is out of range.
        bool Update(const SceneCameraData& data, uint32_t frameIndex);

        // CPU copy of the active camera data, the mapped memory is write combined so it is never read back
        const SceneCameraData& GetData() const { return m_data; }

        // GPU address of the active slot, bind it as a root constant buffer view
        D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const;

      private:
        // Constant buffer views have to start on a 256 byte boundary
        static constexpr uint64_t SLOT_SIZE = (sizeof(SceneCameraData) + 255) & ~static_cast<uint64_t>(255);

        static constexpr uint32_t SLOT_COUNT = BACK_BUFFER_COUNT;

        std::unique_ptr<D3D12Resource> m_resource;
        uint8_t* m_mappedData = nullptr;

        SceneCameraData m_data{};

        uint32_t m_activeSlot{};
    };
} // namespace Wild
