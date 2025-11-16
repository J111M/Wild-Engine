#pragma once

#include "Tools/States.hpp"
#include "Tools/View3d12.hpp"

#include <string>

namespace Wild {
	struct TextureDesc
	{
		enum UsageFlag : uint32_t
		{
			cpuWrite = 1 << 1,
			gpuOnly = 1 << 2,
		};

		enum ViewFlag : uint32_t
		{
			default = 1 << 0,
			shaderResource = 1 << 1,
			renderTarget = 1 << 2,
			depthStencil = 1 << 3,
		};

		TextureType type = TextureType::TEXTURE_2D;
		UsageFlag usage = cpuWrite;
		uint32_t width{ 0 };
		uint32_t height{ 0 };
		uint32_t slices{ 0 };
		uint32_t mips{ 0 };

		ViewFlag flag = default;

		// Data type is unkown at this point
		void* data = nullptr;

		std::string name = "default";

		bool shouldGenerateMips = false;
	};

	class Texture
	{
	public:
		Texture(const TextureDesc& desc);
		Texture(const TextureDesc& desc, ComPtr<ID3D12Resource2> resource);
		~Texture() {};

		ComPtr<ID3D12Resource2> GetResource() { return m_resource; }
		TextureDesc GetDesc() { return m_desc; }

		uint32_t Width() const { return m_desc.width; }
		uint32_t Height() const { return m_desc.height; }

		std::shared_ptr<RenderTargetView> GetRtv() const;
		std::shared_ptr<DepthStencilView> GetDsv() const;

	private:
		ComPtr<ID3D12Resource2> m_resource;

		std::shared_ptr<RenderTargetView> m_rtv;
		std::shared_ptr<DepthStencilView> m_dsv;

		TextureDesc m_desc;
	};
}