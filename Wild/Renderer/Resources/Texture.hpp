#pragma once

#include "Tools/Types3d12.hpp"

#include <string>

namespace Wild {
	enum UsageFlag : uint32_t
	{
		cpu_write = 1 << 1,
		gpu_only = 1 << 2,
	};

	enum ViewFlag : uint32_t
	{
		default = 1 << 0,
		shader_resource = 1 << 1,
		render_target = 1 << 2,
		depth_stencil = 1 << 3,
	};

	struct TextureDesc
	{
		TextureType type = TextureType::TEXTURE_2D;
		uint32_t width{ 0 };
		uint32_t height{ 0 };
		uint32_t slices{ 0 };
		uint32_t mips{ 0 };

		// Data type is unkown at this point
		void* data = nullptr;

		std::string name = "default";

		bool should_generate_mips = false;
	};

	class Texture
	{
	public:
		Texture(const TextureDesc& desc);
		~Texture();

		TextureDesc get_desc() { return desc; }

		uint32_t width() const { return desc.width; }
		uint32_t height() const { return desc.height; }

	private:
		/*ComPtr<D3D12DepthStencilView> m_dsv = nullptr;
		ComPtr<D3D12RenderTargetView> m_rtv = nullptr;*/

		TextureDesc desc;
	};
}