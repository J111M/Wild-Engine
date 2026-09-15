#pragma once

#include "Renderer/ShaderPipeline.hpp"
#include "Tools/D3D12Common.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

// Inspired by Adria engine https://github.com/mateeeeeee/Adria/blob/master/Adria/Graphics/GfxStates.h

namespace Wild
{
    enum class TextureType
    {
        TEXTURE_3D,
        TEXTURE_2D,
        TEXTURE_1D,
        CUBEMAP,
        SKYBOX
    };

    // enum BarrierType : uint32_t
    //{
    //	none = 0,
    //	uav = 1 << 1,
    // };

    enum class PrimitiveTopology : uint8_t
    {
        Undefined,
        TriangleList,
        TriangleStrip,
        PointList,
        LineList,
        LineStrip
    };

    enum class ComparisonFunc : uint8_t
    {
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always,
    };

    enum class DepthWriteMask : uint8_t
    {
        Zero,
        All,
    };

    enum class StencilOp : uint8_t
    {
        Keep,
        Zero,
        Replace,
        IncrSat,
        DecrSat,
        Invert,
        Incr,
        Decr,
    };
    enum class Blend : uint8_t
    {
        Zero,
        One,
        SrcColor,
        InvSrcColor,
        SrcAlpha,
        InvSrcAlpha,
        DstAlpha,
        InvDstAlpha,
        DstColor,
        InvDstColor,
        SrcAlphaSat,
        BlendFactor,
        InvBlendFactor,
        Src1Color,
        InvSrc1Color,
        Src1Alpha,
        InvSrc1Alpha,
    };

    enum class BlendOp : uint8_t
    {
        Add,
        Subtract,
        RevSubtract,
        Min,
        Max,
    };

    enum class FillMode : uint8_t
    {
        Wireframe,
        Solid,
    };

    enum class CullMode : uint8_t
    {
        None,
        Front,
        Back,
    };

    enum class WindingOrder : uint8_t
    {
        Clockwise,
        CounterClockwise,
    };

    enum class ColorWrite
    {
        Disable = 0,
        EnableRed = 1 << 0,
        EnableGreen = 1 << 1,
        EnableBlue = 1 << 2,
        EnableAlpha = 1 << 3,
        EnableAll = ~0,
    };

    struct RenderTargetBlendState
    {
        bool blendEnable = false;
        Blend srcBlend = Blend::One;
        Blend destBlend = Blend::Zero;
        BlendOp blendOperation = BlendOp::Add;
        Blend srcBlendAlpha = Blend::One;
        Blend destBlendAlpha = Blend::Zero;
        BlendOp blendOperationAlpha = BlendOp::Add;
        ColorWrite renderTargetWriteMask = ColorWrite::EnableAll;
    };

    struct BlendState
    {
        bool alphaToCoverageEnable = false;
        bool independentBlendEnable = false;

        RenderTargetBlendState renderTarget[8];
    };

    struct Viewport
    {
        glm::vec2 pos{};
        glm::vec2 size{};
    };

    struct RasterizerState
    {
        PrimitiveTopology topologyMode = PrimitiveTopology::TriangleList;
        FillMode fillMode = FillMode::Solid;
        CullMode cullMode = CullMode::Back;
        WindingOrder windingMode = WindingOrder::CounterClockwise;

        Viewport viewport{};

        BlendState blendDesc;
    };

    struct DepthStencilOp
    {
        StencilOp stencilFailOp = StencilOp::Keep;
        StencilOp stencilDepthFailOp = StencilOp::Keep;
        StencilOp stencilPassOp = StencilOp::Keep;
        ComparisonFunc stencilFunc = ComparisonFunc::Always;
    };

    struct DepthStencilState
    {
        bool depthEnable = true;
        DepthWriteMask depthWriteMask = DepthWriteMask::All;
        ComparisonFunc depthFunc = ComparisonFunc::LessEqual;
        bool stencilEnable = false;
        uint8_t stencilReadMask = 0xff;
        uint8_t stencilWriteMask = 0xff;

        DepthStencilOp frontFace{};
        DepthStencilOp backFace{};
    };

    struct RaytracingState
    {
        size_t payloadSize{};
        size_t attributeSize{};
        uint32_t rayRecursionDepth{};
    };

    struct ShaderState
    {
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

        std::shared_ptr<Shader> vertexShader;
        std::shared_ptr<Shader> fragShader;

        std::shared_ptr<Shader> computeShader;

        // Mesh shader and amplification shader
        std::shared_ptr<Shader> amplificationShader;
        std::shared_ptr<Shader> meshShader;

        std::shared_ptr<Shader> rayTracingShader;
    };

    struct PipelineStateSettings
    {
        // Shader data and vertex layout
        ShaderState shaderState{};
        RaytracingState raytracingState{};
        RasterizerState rasterizerState{};
        DepthStencilState depthStencilState{};

        std::vector<DXGI_FORMAT> renderTargetsFormat{};
        DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;

        std::string pipelineName{};
    };

    enum class ClearOperation
    {
        Clear = 0,
        Store = 1
    };

    enum class DSClearOperation
    {
        DepthClear = 0,
        StencilClear = 1,
        Store = 2,
        ClearAll = 3
    };
} // namespace Wild

namespace RootParams
{
    enum RootResourceType : uint8_t
    {
        Constants,
        ConstantBufferView,
        ShaderResourceView,
        UnorderedAccessView,
        DescriptorTable, // For SRV, UAV and CBV table
        StaticSampler,
    };
} // namespace RootParams
