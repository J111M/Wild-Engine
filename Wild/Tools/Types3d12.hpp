#pragma once

#include "Tools/Common3d12.hpp"

namespace Wild {
    enum class TextureType
    {
        TEXTURE_2D = 0,
        CUBEMAP
    };

    struct PipelineStateSettings {

        D3D12_GRAPHICS_PIPELINE_STATE_DESC stateStream;


        std::string name{};
    };
}