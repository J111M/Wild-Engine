#pragma once

#include "Renderer/Resources/Resource3d12.hpp"

#include "Tools/States.hpp"
#include "Tools/View3d12.hpp"
#include "Tools/Common3d12.hpp"

#include <string>
#include <filesystem>

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
			none = 0,
			shaderResource = 1 << 1,
			renderTarget = 1 << 2,
			depthStencil = 1 << 3,
			readWrite = 1 << 4,
		};

		TextureType type = TextureType::TEXTURE_2D;
		UsageFlag usage = cpuWrite;
		uint32_t width{ 0 };
		uint32_t Height{ 0 };
		uint32_t Depth{ 1 };
		uint32_t Layers{ 1 };
		uint32_t mips{ 1 };

		// TODO make abstracted format type
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;

		ViewFlag flag = none;

		// Bool to make sure texture is not deleted when the cache is flushed
		bool shouldStayInCache = false;

		std::string name = "default";
	};

	class Texture
	{
	public:
		// For loading texture from disk
		Texture(const std::string& filePath, TextureType type = TextureType::TEXTURE_2D, uint32_t mips = 0, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
		Texture(const TextureDesc& desc);
		// For swapchain rendertargets
		Texture(const TextureDesc& desc, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES initialState);
		~Texture() {};

		void Transition(CommandList& list, D3D12_RESOURCE_STATES newState);
		//void Barrier(CommandList& list, BarrierType type);

		ID3D12Resource* GetResource() { return m_resource->Handle().Get(); }
		TextureDesc GetDesc() { return m_desc; }

		uint32_t Width() const { return m_desc.width; }
		uint32_t Height() const { return m_desc.Height; }

		std::shared_ptr<RenderTargetView> GetRtv() const;
		std::shared_ptr<RenderTargetView> GetRtv(uint32_t index) const;
		const bool UsesRtvArray() const { return m_rtvArrayAvailiable; };

		std::shared_ptr<UnorderedAccessView> GetUav() const;
		std::shared_ptr<UnorderedAccessView> GetUav(uint32_t index) const;
		const bool UsesUavArray() const { return m_uavArrayAvailiable; };

		std::shared_ptr<DepthStencilView> GetDsv() const;
		std::shared_ptr<ShaderResourceView> GetSrv() const;
		
	private:
		void CreateTexture(const std::string& filePath);
		void CreateCubeMapTexture(const std::string& filePath);
		void CreateSkyboxTexture(const std::string& filePath);

		std::unique_ptr<Resource> m_resource;

		std::shared_ptr<RenderTargetView> m_rtv;

		// Rtv support for rendering to a cubemap texture
		std::vector<std::shared_ptr<RenderTargetView>> m_rtvArray;
		bool m_rtvArrayAvailiable = false;

		std::shared_ptr<UnorderedAccessView> m_uav;

		// Uav support for cubemap textures
		std::vector<std::shared_ptr<UnorderedAccessView>> m_uavArray;
		bool m_uavArrayAvailiable = false;

		std::shared_ptr<DepthStencilView> m_dsv;
		std::shared_ptr<ShaderResourceView> m_srv;

		TextureDesc m_desc;
	};
}