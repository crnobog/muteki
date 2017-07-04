#pragma once

#include "mu-core/Debug.h"
#include "mu-core/PrimitiveTypes.h"
#include "mu-core/RefCountPtr.h"

#include "../GPU_Common.h"

#include <d3d12.h>

#define EnsureHR(expr) \
	do { \
		HRESULT hr =(expr); if(SUCCEEDED(hr)) {break;} \
		CHECKF(SUCCEEDED(hr), #expr, "failed with error code ", hr); \
	} while(false);

namespace GPU_DX12 {
	template<typename OBJECT>
	struct COMRefCount {
		static void IncRef(OBJECT* o) {
			o->AddRef();
		}
		static void DecRef(OBJECT* o) {
			o->Release();
		}
	};

	template<typename COMObject>
	using COMPtr = RefCountPtr<COMObject, COMRefCount>;

	enum class ResourceState : i32 {
		Present = D3D12_RESOURCE_STATE_PRESENT,
		RenderTarget = D3D12_RESOURCE_STATE_RENDER_TARGET,
		GenericRead = D3D12_RESOURCE_STATE_GENERIC_READ,
		CopyDest = D3D12_RESOURCE_STATE_COPY_DEST,
		CopySource = D3D12_RESOURCE_STATE_COPY_SOURCE,
		PixelShaderResource = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		Common = D3D12_RESOURCE_STATE_COMMON,
	};

	enum class PrimType {
		Triangle = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
	};

	struct ResourceBarrier : D3D12_RESOURCE_BARRIER {
		ResourceBarrier(ID3D12Resource* resource, ResourceState before, ResourceState after);
	};

	struct BufferDesc : D3D12_RESOURCE_DESC {
		BufferDesc(size_t width);
	};

	struct TextureDesc2D : D3D12_RESOURCE_DESC {
		TextureDesc2D(u32 width, u32 height, DXGI_FORMAT format);
	};

	struct TextureViewDesc2D : D3D12_SHADER_RESOURCE_VIEW_DESC {
		TextureViewDesc2D(DXGI_FORMAT format);
	};

	struct TextureCopyLocation : D3D12_TEXTURE_COPY_LOCATION {
		TextureCopyLocation(ID3D12Resource* resource, u32 subresource) {
			pResource = resource;
			Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			SubresourceIndex = subresource;
		}
		TextureCopyLocation(ID3D12Resource* resource, D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint) {
			pResource = resource;
			Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			PlacedFootprint = footprint;
		}
	};

	extern const D3D12_BLEND_DESC DefaultBlendDesc;
	extern const D3D12_RASTERIZER_DESC DefaultRasterizerState;
	extern const D3D12_DEPTH_STENCIL_DESC DefaultDepthStencilState;

	struct GraphicsPipelineStateDesc : D3D12_GRAPHICS_PIPELINE_STATE_DESC {
		typedef D3D12_GRAPHICS_PIPELINE_STATE_DESC Base;
		GraphicsPipelineStateDesc(ID3D12RootSignature* root_signature);
		GraphicsPipelineStateDesc& VS(ID3DBlob* blob);
		GraphicsPipelineStateDesc& PS(ID3DBlob* blob);
		GraphicsPipelineStateDesc& DepthEnable(bool enable);
		GraphicsPipelineStateDesc& PrimType(PrimType type);
		GraphicsPipelineStateDesc& RenderTargets(DXGI_FORMAT format);
		GraphicsPipelineStateDesc& InputLayout(D3D12_INPUT_ELEMENT_DESC* elements, u32 num_elements);
	};


	inline D3D12_PRIMITIVE_TOPOLOGY CommonToDX12(GPUCommon::PrimitiveTopology pt) {
		static D3D12_PRIMITIVE_TOPOLOGY list[] = {
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
		};
		return list[(i32)pt];
	}

	inline DXGI_FORMAT CommonToDX12(GPUCommon::TextureFormat format) {
		switch (format) {
		case GPUCommon::TextureFormat::RGBA8:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		}
		CHECK(false);
		return DXGI_FORMAT_UNKNOWN;
	}

	inline void MemcpySubresource(
		const D3D12_MEMCPY_DEST* pDest,
		const D3D12_SUBRESOURCE_DATA* pSrc,
		size_t RowSizeInBytes,
		u32 NumRows,
		u32 NumSlices) {
		for (u32 z = 0; z < NumSlices; ++z) {
			u8* pDestSlice = reinterpret_cast<u8*>(pDest->pData) + pDest->SlicePitch * z;
			const u8* pSrcSlice = reinterpret_cast<const u8*>(pSrc->pData) + pSrc->SlicePitch * z;
			for (u32 y = 0; y < NumRows; ++y) {
				memcpy(pDestSlice + pDest->RowPitch * y,
					pSrcSlice + pSrc->RowPitch * y,
					RowSizeInBytes);
			}
		}
	}

	inline u32 CalcRowPitch(GPUCommon::TextureFormat format, u32 width) {
		switch (format) {
		case GPUCommon::TextureFormat::RGBA8:
			return 4 * width;
		}		
		CHECK(false);
		return 0;
	}
}