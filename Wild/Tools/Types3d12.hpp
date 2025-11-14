#pragma once

#include "Renderer/ShaderPipeline.hpp"
#include "Tools/Common3d12.hpp"

#include <vector>
#include <memory>

// Inspired by Adria engine https://github.com/mateeeeeee/Adria/blob/master/Adria/Graphics/GfxStates.h

namespace Wild {
    enum class TextureType
    {
        TEXTURE_2D = 0,
        CUBEMAP
    };

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
		bool BlendEnable = false;
		Blend SrcBlend = Blend::One;
		Blend DestBlend = Blend::Zero;
		BlendOp BlendOperation = BlendOp::Add;
		Blend SrcBlendAlpha = Blend::One;
		Blend DestBlendAlpha = Blend::Zero;
		BlendOp BlendOperationAlpha = BlendOp::Add;
		ColorWrite RenderTargetWriteMask = ColorWrite::EnableAll;
	};

	struct BlendState
	{
		bool AlphaToCoverageEnable = false;
		bool IndependentBlendEnable = false;

		RenderTargetBlendState RenderTarget[8];
	};

    struct RasterizerState {
		FillMode FillMode = FillMode::Solid;
		CullMode CullMode = CullMode::Back;
		WindingOrder WindingMode = WindingOrder::CounterClockwise;

		BlendState BlendDesc;
    };

	struct DepthStencilOp
	{
		StencilOp stencil_fail_op = StencilOp::Keep;
		StencilOp stencil_depth_fail_op = StencilOp::Keep;
		StencilOp stencil_pass_op = StencilOp::Keep;
		ComparisonFunc stencil_func = ComparisonFunc::Always;
	};

	struct DepthStencilState
	{
		bool DepthEnable = true;
		DepthWriteMask depth_write_mask = DepthWriteMask::All;
		ComparisonFunc depth_func = ComparisonFunc::Greater;
		bool stencil_enable = false;
		uint8_t stencil_read_mask = 0xff;
		uint8_t stencil_write_mask = 0xff;
		
		DepthStencilOp FrontFace{};
		DepthStencilOp BackFace{};
	};

	struct ShaderState {
		std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

		std::shared_ptr<Shader> VertexShader;
		std::shared_ptr<Shader> FragShader;
		std::shared_ptr<Shader> ComputeShader;
	};

    struct PipelineStateSettings {
		ShaderState ShaderState{};

		RasterizerState RasterizerState{};
		DepthStencilState DepthStencilState{};

		ComPtr<ID3D12RootSignature> RootSignature{};

        std::string PipelineName{};
    };
}