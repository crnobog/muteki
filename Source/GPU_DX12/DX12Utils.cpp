#include "DX12Utils.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

namespace DX12Util {
	ResourceBarrier::ResourceBarrier(ID3D12Resource* resource, ResourceState before, ResourceState after) {
		Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		Transition.pResource = resource;
		Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		Transition.StateBefore = (D3D12_RESOURCE_STATES)before;
		Transition.StateAfter = (D3D12_RESOURCE_STATES)after;
	}


	BufferDesc::BufferDesc(size_t width) {
		Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		Alignment = 0;
		Width = (u32)width;
		Height = 1;
		DepthOrArraySize = 1;
		MipLevels = 1;
		Format = DXGI_FORMAT_UNKNOWN;
		SampleDesc = { 1, 0 };
		Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Flags = D3D12_RESOURCE_FLAG_NONE;
	}

	TextureDesc2D::TextureDesc2D(u32 width, u32 height, DXGI_FORMAT format) {
		Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		Alignment = 0;
		Width = width;
		Height = height;
		DepthOrArraySize = 1;
		MipLevels = 1;
		Format = format;
		SampleDesc.Count = 1;
		SampleDesc.Quality = 0;
		Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		Flags = D3D12_RESOURCE_FLAG_NONE;
	}

	TextureViewDesc2D::TextureViewDesc2D(DXGI_FORMAT format) {
		Format = format;
		ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		Texture2D = D3D12_TEX2D_SRV{0, 1, 0, 0};
	}

	GraphicsPipelineStateDesc::GraphicsPipelineStateDesc(ID3D12RootSignature* root_signature) {
		Base::pRootSignature = root_signature;
		Base::VS = { 0, 0 };
		Base::PS = { 0, 0 };
		Base::DS = { 0, 0 };
		Base::HS = { 0, 0 };
		Base::GS = { 0, 0 };
		Base::StreamOutput = { nullptr, 0, nullptr, 0, 0 };
		Base::BlendState = DefaultBlendDesc;
		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
			Base::RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
		}
		Base::SampleMask = UINT_MAX;
		Base::RasterizerState = DefaultRasterizerState;
		Base::DepthStencilState = DefaultDepthStencilState;
		Base::InputLayout = { nullptr, 0 };
		Base::IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		Base::PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		Base::NumRenderTargets = 0;
		Base::DSVFormat = DXGI_FORMAT_UNKNOWN;
		Base::SampleDesc = { 1, 0 };
		Base::NodeMask = 0;
		Base::CachedPSO = { nullptr, 0 };
		Base::Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	}

	GraphicsPipelineStateDesc& GraphicsPipelineStateDesc::VS(ID3DBlob* blob) {
		Base::VS = { blob->GetBufferPointer(), blob->GetBufferSize() };
		return *this;
	}
	GraphicsPipelineStateDesc& GraphicsPipelineStateDesc::PS(ID3DBlob* blob) {
		Base::PS = { blob->GetBufferPointer(), blob->GetBufferSize() };
		return *this;
	}
	GraphicsPipelineStateDesc& GraphicsPipelineStateDesc::DepthEnable(bool enable) {
		Base::DepthStencilState.DepthEnable = enable;
		return *this;
	}
	GraphicsPipelineStateDesc& GraphicsPipelineStateDesc::PrimType(DX12Util::PrimType type) {
		Base::PrimitiveTopologyType = (D3D12_PRIMITIVE_TOPOLOGY_TYPE)type;
		return *this;
	}
	GraphicsPipelineStateDesc& GraphicsPipelineStateDesc::RenderTargets(DXGI_FORMAT format) {
		Base::NumRenderTargets = 1;
		Base::RTVFormats[0] = format;
		return *this;
	}
	GraphicsPipelineStateDesc& GraphicsPipelineStateDesc::InputLayout(const D3D12_INPUT_ELEMENT_DESC* elements, u32 num_elements) {
		Base::InputLayout.NumElements = num_elements;
		Base::InputLayout.pInputElementDescs = elements;
		return *this;
	}
	GraphicsPipelineStateDesc& GraphicsPipelineStateDesc::BlendState(const GPU::BlendStateDesc& desc) {
		Base::BlendState.AlphaToCoverageEnable = desc.AlphaToCoverageEnable;
		Base::BlendState.IndependentBlendEnable = false;
		Base::BlendState.RenderTarget[0] = D3D12_RENDER_TARGET_BLEND_DESC{
			desc.BlendEnable, false,
			CommonToDX12(desc.ColorBlend.Source), CommonToDX12(desc.ColorBlend.Dest),CommonToDX12(desc.ColorBlend.Op),
			CommonToDX12(desc.AlphaBlend.Source), CommonToDX12(desc.AlphaBlend.Dest),CommonToDX12(desc.AlphaBlend.Op),
			D3D12_LOGIC_OP_NOOP,
			(UINT8)0xF,
		};
		return *this;
	}


	static_assert(D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT == 8, "D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT != 8");
	const D3D12_BLEND_DESC DefaultBlendDesc = { false, false,{
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		},
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		},
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		},
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		},
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		},
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		},
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		},
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		},
	} };


	const D3D12_RASTERIZER_DESC DefaultRasterizerState = {
		D3D12_FILL_MODE_SOLID,
		D3D12_CULL_MODE_BACK,
		false,
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		true,
		false,
		false,
		0,
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	};

	const D3D12_DEPTH_STENCIL_DESC DefaultDepthStencilState = {
		true,
		D3D12_DEPTH_WRITE_MASK_ALL,
		D3D12_COMPARISON_FUNC_LESS,
		false,
		D3D12_DEFAULT_STENCIL_READ_MASK,
		D3D12_DEFAULT_STENCIL_WRITE_MASK,
		{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS },
		{ D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS }
	};


	DX::VertexShaderInputElement ParseInputParameter(D3D12_SIGNATURE_PARAMETER_DESC& input_param) {
		std::tuple<GPU::InputSemantic, const char*> table[] = {
			{ GPU::InputSemantic::Position, "POSITION" },
			{ GPU::InputSemantic::Color, "COLOR" },
			{ GPU::InputSemantic::Texcoord, "TEXCOORD" },
			{ GPU::InputSemantic::Normal, "NORMAL" },
		};
		auto found = mu::Find(mu::Range(table), [&](const std::tuple<GPU::InputSemantic, const char*>& sem) {
			return strcmp(std::get<1>(sem), input_param.SemanticName) == 0;
		});
		CHECK(!found.IsEmpty());
		DX::VertexShaderInputElement out_elem;
		out_elem.Semantic = std::get<0>(found.Front());
		out_elem.SemanticIndex = input_param.SemanticIndex;
		out_elem.Type = GPU::ScalarType::Float;
		out_elem.CountMinusOne = 3;
		return out_elem;
	}
}