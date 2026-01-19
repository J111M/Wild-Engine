#include "Renderer/Resources/Texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Wild {
	Texture::Texture(const std::string& filePath, TextureType type, uint32_t mips, DXGI_FORMAT format)
	{
		m_desc.name = filePath;
		m_desc.type = type;
		m_desc.format = format;

			switch (type)
			{
			case Wild::TextureType::TEXTURE_2D:
				CreateTexture(filePath);
				break;
			case Wild::TextureType::TEXTURE_1D:
				WD_WARN("Not supported yet.");
				// CreateTexture(filePath);
				break;
			case Wild::TextureType::CUBEMAP:
				CreateCubeMapTexture(filePath);
				break;
			case Wild::TextureType::SKYBOX:
				CreateSkyboxTexture(filePath);
				break;
			default:
				break;
			}

		m_resource->Handle()->SetName(StringToWString(m_desc.name).c_str());
	}

	Texture::Texture(const TextureDesc& desc)
	{
		m_desc = desc;

		auto gfxContext = engine.GetGfxContext();

		if (m_desc.flag & TextureDesc::renderTarget) {
			D3D12_RESOURCE_DESC textureDesc = {};
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			textureDesc.Width = m_desc.width;
			textureDesc.Height = m_desc.Height;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.MipLevels = 1;
			textureDesc.Format = m_desc.format;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.SampleDesc.Quality = 0;
			textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE clearValue = {};
			clearValue.Format = m_desc.format;
			clearValue.Color[0] = 0.0f;
			clearValue.Color[1] = 0.0f;
			clearValue.Color[2] = 0.0f;
			clearValue.Color[3] = 1.0f;

			// Create resource with initial state
			m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_RENDER_TARGET);

			ThrowIfFailed(gfxContext->GetDevice()->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&textureDesc,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				&clearValue,
				IID_PPV_ARGS(&m_resource->Handle())
			), "Failed to create RTV");

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.Format = m_desc.format;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

			m_rtv = std::make_shared<RenderTargetView>(m_resource->Handle(), rtvDesc);
		}

		if (desc.flag & TextureDesc::depthStencil) {
			D3D12_RESOURCE_DESC depthDesc = {};
			depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			depthDesc.Alignment = 0;
			depthDesc.Width = m_desc.width;
			depthDesc.Height = m_desc.Height;
			depthDesc.DepthOrArraySize = 1;
			depthDesc.MipLevels = 1;
			depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
			depthDesc.SampleDesc.Count = 1;
			depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_CLEAR_VALUE clearValue = {};
			clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			clearValue.DepthStencil = { 1.0f, 0 };

			// Create resource with initial state
			m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_DEPTH_WRITE);

			gfxContext->GetDevice()->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&depthDesc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&clearValue,
				IID_PPV_ARGS(&m_resource->Handle())
			);

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

			m_dsv = std::make_shared<DepthStencilView>(m_resource->Handle(), dsvDesc);

			m_desc.format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		}

		if (m_desc.flag & TextureDesc::ViewFlag::shaderResource) {
			D3D12_SHADER_RESOURCE_VIEW_DESC  srvDesc = {};
			srvDesc.Format = m_desc.format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.MostDetailedMip = 0;

			m_srv = std::make_shared<ShaderResourceView>(m_resource->Handle(), srvDesc);
		}

		m_resource->Handle()->SetName(StringToWString(m_desc.name).c_str());
	}

	Texture::Texture(const TextureDesc& desc, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES initialState)
	{
		m_desc = desc;
		m_resource = std::make_unique<Resource>(initialState);
		m_resource->SetResource(resource);

		if (desc.flag & TextureDesc::renderTarget) {
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

			m_rtv = std::make_shared<RenderTargetView>(m_resource->Handle(), rtvDesc);
		}

		if (desc.flag & TextureDesc::depthStencil) {
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

			m_dsv = std::make_shared<DepthStencilView>(m_resource->Handle(), dsvDesc);
		}
	}

	void Texture::Transition(CommandList& list, D3D12_RESOURCE_STATES newState) {
		m_resource->Transition(list, newState);
	}

	std::shared_ptr<RenderTargetView> Texture::GetRtv() const
	{
		if (!m_rtv) {
			WD_ERROR("rtv not availiable on this texture.");
			return nullptr;
		}

		return m_rtv;
	}

	std::shared_ptr<DepthStencilView> Texture::GetDsv() const
	{
		if (!m_dsv) {
			WD_ERROR("dsv not availiable on this texture.");
			return nullptr;
		}

		return m_dsv;
	}

	std::shared_ptr<ShaderResourceView> Texture::GetSrv() const
	{
		if (!m_srv) {
			WD_ERROR("m_srv not availiable on this texture.");
			return nullptr;
		}

		return m_srv;
	}

	void Texture::CreateTexture(const std::string& filePath)
	{
		auto& gfxContext = engine.GetGfxContext();

		int width, height, channels;
		stbi_uc* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		uint32_t imageSize = width * height * channels;

		if (!pixels)
		{
			WD_ERROR("No file exists at the file path.");
			return;
		}

		m_desc.width = static_cast<uint32_t>(width);
		m_desc.Height = static_cast<uint32_t>(height);

		D3D12_RESOURCE_DESC textureDesc{};

		textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			m_desc.format,
			static_cast<UINT64>(m_desc.width),
			static_cast<UINT>(m_desc.Height),
			static_cast<UINT16>(m_desc.Layers),
			static_cast<UINT16>(m_desc.mips));

		// Create resource with copy dest since update subresource will transition it to that
		m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_COPY_DEST);

		ThrowIfFailed(gfxContext->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&m_resource->Handle())));

		// Create Upload heap
		UINT64 uploadBufferSize = GetRequiredIntermediateSize(GetResource(), 0, 1);

		ComPtr<ID3D12Resource> uploadHeap;
		ThrowIfFailed(gfxContext->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadHeap)));

		D3D12_SUBRESOURCE_DATA subrsData{};
		subrsData.pData = pixels;
		subrsData.RowPitch = m_desc.width * 4;
		subrsData.SlicePitch = subrsData.RowPitch * m_desc.Height;

		CommandList list(D3D12_COMMAND_LIST_TYPE_DIRECT);

		UpdateSubresources(list.GetList().Get(), GetResource(), uploadHeap.Get(), 0, 0, 1, &subrsData);

		m_resource->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		list.Close();
		engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->ExecuteList(list);
		engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->WaitForFence();

		stbi_image_free(pixels);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = m_desc.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;

		m_srv = std::make_shared<ShaderResourceView>(m_resource->Handle(), srvDesc);
	}

	void Texture::CreateCubeMapTexture(const std::string& filePath)
	{

	}

	void Texture::CreateSkyboxTexture(const std::string& filePath)
	{
		auto& gfxContext = engine.GetGfxContext();

		int width, height, channels;
		float* pixels = stbi_loadf(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

		if (!pixels)
		{
			WD_ERROR("No data could be loaded.");
			return;
		}

		m_desc.width = static_cast<uint32_t>(width);
		m_desc.Height = static_cast<uint32_t>(height);

		D3D12_RESOURCE_DESC textureDesc{};

		textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			m_desc.format,
			static_cast<UINT64>(m_desc.width),
			static_cast<UINT>(m_desc.Height),
			static_cast<UINT16>(m_desc.Layers),
			static_cast<UINT16>(m_desc.mips));

		// Create resource with copy dest since update subresource will transition it to that
		m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_COPY_DEST);

		ThrowIfFailed(gfxContext->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&m_resource->Handle())));

		// Create Upload heap
		UINT64 uploadBufferSize = GetRequiredIntermediateSize(GetResource(), 0, 1);

		ComPtr<ID3D12Resource> uploadHeap;
		ThrowIfFailed(gfxContext->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadHeap)));

		D3D12_SUBRESOURCE_DATA subrsData{};
		subrsData.pData = pixels;
		subrsData.RowPitch = m_desc.width * 4 * sizeof(float);
		subrsData.SlicePitch = subrsData.RowPitch * m_desc.Height;

		CommandList list(D3D12_COMMAND_LIST_TYPE_DIRECT);

		UpdateSubresources(list.GetList().Get(), GetResource(), uploadHeap.Get(), 0, 0, 1, &subrsData);

		m_resource->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		list.Close();
		engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->ExecuteList(list);
		engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->WaitForFence();

		stbi_image_free(pixels);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = m_desc.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;

		m_srv = std::make_shared<ShaderResourceView>(m_resource->Handle(), srvDesc);
	}
}