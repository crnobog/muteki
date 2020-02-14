#pragma once

#include "mu-core/Debug.h"
#include "mu-core/PrimitiveTypes.h"
#include "mu-core/RefCountPtr.h"
#include "mu-core/PointerRange.h"

#include "../GPUInterface.h"
#include "../GPU_DXShared/DXSharedUtils.h"

#include <d3d12.h>
#include <d3d12shader.h>


namespace DX12Util {

	enum class ResourceState : i32 {
		Present = D3D12_RESOURCE_STATE_PRESENT,
		RenderTarget = D3D12_RESOURCE_STATE_RENDER_TARGET,
		GenericRead = D3D12_RESOURCE_STATE_GENERIC_READ,
		CopyDest = D3D12_RESOURCE_STATE_COPY_DEST,
		CopySource = D3D12_RESOURCE_STATE_COPY_SOURCE,
		PixelShaderResource = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		Common = D3D12_RESOURCE_STATE_COMMON,
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

	struct DepthBufferDesc : D3D12_RESOURCE_DESC {
		DepthBufferDesc(u32 width, u32 height, DXGI_FORMAT format);
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

	// TODO: This feels unnecessary, get rid of it
	struct GraphicsPipelineStateDesc : D3D12_GRAPHICS_PIPELINE_STATE_DESC {
		typedef D3D12_GRAPHICS_PIPELINE_STATE_DESC Base;
		GraphicsPipelineStateDesc(ID3D12RootSignature* root_signature);
		GraphicsPipelineStateDesc& VS(ID3DBlob* blob);
		GraphicsPipelineStateDesc& PS(ID3DBlob* blob);
		GraphicsPipelineStateDesc& DepthStencilState(const GPU::DepthStencilStateDesc& desc);
		GraphicsPipelineStateDesc& DepthStencilFormat(DXGI_FORMAT format);
		GraphicsPipelineStateDesc& PrimType(GPU::PrimitiveType type);
		GraphicsPipelineStateDesc& RenderTargets(DXGI_FORMAT format);
		GraphicsPipelineStateDesc& InputLayout(const D3D12_INPUT_ELEMENT_DESC* elements, u32 num_elements);
		GraphicsPipelineStateDesc& BlendState(const GPU::BlendStateDesc& desc);
		GraphicsPipelineStateDesc& RasterState(const GPU::RasterStateDesc& desc);
	};

	inline D3D12_COMPARISON_FUNC CommonToDX12(GPU::ComparisonFunc func) {
		static D3D12_COMPARISON_FUNC list[] = {
			D3D12_COMPARISON_FUNC_NEVER,
			D3D12_COMPARISON_FUNC_LESS,
			D3D12_COMPARISON_FUNC_EQUAL,
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_COMPARISON_FUNC_GREATER,
			D3D12_COMPARISON_FUNC_NOT_EQUAL,
			D3D12_COMPARISON_FUNC_GREATER_EQUAL,
			D3D12_COMPARISON_FUNC_ALWAYS
		};
		return list[(i32)func];
	}

	inline D3D12_STENCIL_OP CommonToDX12(GPU::StencilOp op) {
		static D3D12_STENCIL_OP list[] = {
			D3D12_STENCIL_OP_KEEP,
			D3D12_STENCIL_OP_ZERO,
			D3D12_STENCIL_OP_REPLACE,
			D3D12_STENCIL_OP_INCR_SAT,
			D3D12_STENCIL_OP_DECR_SAT,
			D3D12_STENCIL_OP_INVERT,
			D3D12_STENCIL_OP_INCR,
			D3D12_STENCIL_OP_DECR
		};
		return list[(i32)op];
	}

	inline D3D12_DEPTH_STENCILOP_DESC CommonToDX12(GPU::StencilOpDesc desc) {
		return {
			CommonToDX12(desc.StencilFailOp),
			CommonToDX12(desc.StencilDepthFailOp),
			CommonToDX12(desc.StencilPassOp),
			CommonToDX12(desc.StencilFunc)
		};
	}

	inline D3D12_PRIMITIVE_TOPOLOGY CommonToDX12(GPU::PrimitiveTopology pt) {
		static D3D12_PRIMITIVE_TOPOLOGY list[] = {
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
			D3D_PRIMITIVE_TOPOLOGY_LINELIST,
		};
		return list[(i32)pt];
	}

	inline D3D12_PRIMITIVE_TOPOLOGY_TYPE CommonToDX12(GPU::PrimitiveType pt) {
		static D3D12_PRIMITIVE_TOPOLOGY_TYPE list[] = {
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
		};
		return list[(i32)pt];
	}

	inline DXGI_FORMAT CommonToDX12(GPU::TextureFormat format) {
		i32 i = (i32)format;

		DXGI_FORMAT formats[] = {
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_R8_UNORM,
			DXGI_FORMAT_R8G8_UNORM,
			DXGI_FORMAT_R8G8B8A8_UNORM,
		};
		static_assert(size_t(GPU::TextureFormat::NumFormats) == ArraySize(formats));
		Assert(i >= 0 && i < ArraySize(formats));
		return formats[i];
	}

	inline D3D12_BLEND_OP CommonToDX12(GPU::BlendOp op) {
		switch (op) {
		case GPU::BlendOp::Add:				return D3D12_BLEND_OP_ADD;
		case GPU::BlendOp::Subtract:		return D3D12_BLEND_OP_SUBTRACT;
		case GPU::BlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
		case GPU::BlendOp::Min:				return D3D12_BLEND_OP_MIN;
		case GPU::BlendOp::Max:				return D3D12_BLEND_OP_MAX;
		}
		Assert(false);
		return D3D12_BLEND_OP_ADD;
	}

	inline D3D12_BLEND CommonToDX12(GPU::BlendValue val) {
		switch (val) {
		case GPU::BlendValue::Zero:					return D3D12_BLEND_ZERO;
		case GPU::BlendValue::One:					return D3D12_BLEND_ONE;
		case GPU::BlendValue::SourceColor:			return D3D12_BLEND_SRC_COLOR;
		case GPU::BlendValue::InverseSourceColor:	return D3D12_BLEND_INV_SRC_COLOR;
		case GPU::BlendValue::SourceAlpha:			return D3D12_BLEND_SRC_ALPHA;
		case GPU::BlendValue::InverseSourceAlpha:	return D3D12_BLEND_INV_SRC_ALPHA;
		case GPU::BlendValue::DestAlpha:			return D3D12_BLEND_DEST_ALPHA;
		case GPU::BlendValue::InverseDestAlpha:		return D3D12_BLEND_INV_DEST_ALPHA;
		case GPU::BlendValue::DestColor:			return D3D12_BLEND_DEST_COLOR;
		case GPU::BlendValue::InverseDestColor:		return D3D12_BLEND_INV_DEST_COLOR;
		}
		Assert(false);
		return D3D12_BLEND_ZERO;
	}

	inline D3D12_FILL_MODE CommonToDX12(GPU::FillMode mode) {
		switch (mode) {
		case GPU::FillMode::Wireframe: return D3D12_FILL_MODE_WIREFRAME;
		case GPU::FillMode::Solid: return D3D12_FILL_MODE_SOLID;
		}
		Assert(false);
		return D3D12_FILL_MODE_WIREFRAME;
	}

	inline D3D12_CULL_MODE CommonToDX12(GPU::CullMode mode) {
		switch (mode) {
		case GPU::CullMode::None: return D3D12_CULL_MODE_NONE;
		case GPU::CullMode::Front: return D3D12_CULL_MODE_FRONT;
		case GPU::CullMode::Back: return D3D12_CULL_MODE_BACK;
		}
		Assert(false);
		return D3D12_CULL_MODE_NONE;
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
}
