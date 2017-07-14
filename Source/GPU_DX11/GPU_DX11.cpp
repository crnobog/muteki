#include "mu-core/Debug.h"

#include "../GPUInterface.h"
#include "GPU_DX11.h"
#include "DX11Utils.h"
#include "../GPU_DXShared/DXSharedUtils.h"
#include "../Vectors.h"

#include "mu-core/FixedArray.h"
#include "mu-core/Pool.h"
#include "mu-core/Ranges.h"
#include "mu-core/String.h"
#include "mu-core/Utils.h"

#include <d3d11.h>
#include <dxgi1_4.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#pragma warning(push) 
#pragma warning(disable : 4100)

using StreamFormatID = GPU::StreamFormatID;
using VertexBufferID = GPU::VertexBufferID;
using IndexBufferID = GPU::IndexBufferID;
using InputAssemblerConfigID = GPU::InputAssemblerConfigID;
using VertexShaderID = GPU::VertexShaderID;
using PixelShaderID = GPU::PixelShaderID;
using ProgramID = GPU::ProgramID;
using ConstantBufferID = GPU::ConstantBufferID;
using RenderTargetID = GPU::RenderTargetID;
using TextureID = GPU::TextureID;
using ShaderResourceListID = GPU::ShaderResourceListID;
using RenderPass = GPU::RenderPass;

using DX::COMPtr;

using mu::Array;
using mu::FixedArray;
using mu::Iota;
using mu::Pool;
using mu::String;
using mu::Zip;

using std::tuple;
using std::get;


struct GPU_DX11 : public GPUInterface {

	struct ShaderResourceList {
		GPU::ShaderResourceListDesc Desc;
	};

	struct InputAssemblerConfig {
		StreamFormatID StreamFormat;
		FixedArray<VertexBufferID, GPU::MaxStreamSlots> Slots;
		IndexBufferID IndexBuffer;
	};
	struct StreamFormat {
		FixedArray<u32, GPU::MaxStreamSlots> Strides;
		GPU::StreamFormatDesc Desc;
		COMPtr<ID3D11InputLayout> InputLayout;
	};
	struct Texture {
		COMPtr<ID3D11Texture2D> Resource;
		COMPtr<ID3D11ShaderResourceView> SRV;
	};
	struct VertexShaderInputs {
		static constexpr i32 MaxInputElements = 8;
		FixedArray<DX::VertexShaderInputElement, MaxInputElements> InputElements;
	};
	struct VertexShader {
		COMPtr<ID3D11VertexShader> CompiledShader;
		VertexShaderInputs Inputs;
	};
	struct LinkedProgram {
		VertexShaderID VertexShader;
		PixelShaderID PixelShader;
	};

	GPU_DX11() = default;
	~GPU_DX11() = default;
	GPU_DX11(const GPU_DX11&) = delete;
	void operator=(const GPU_DX11&) = delete;
	GPU_DX11(GPU_DX11&&) = delete;
	void operator=(GPU_DX11&&) = delete;

	static constexpr u32 frame_count = 2;

	COMPtr<IDXGIFactory4>		m_dxgi_factory;
	COMPtr<ID3D11Device>		m_device;
	COMPtr<ID3D11DeviceContext> m_immediate_context;
	
	COMPtr<IDXGISwapChain3>				m_swap_chain;
	COMPtr<ID3D11Texture2D>				m_back_buffer;
	COMPtr<ID3D11RenderTargetView>		m_back_buffer_rtv;

	COMPtr<ID3D11SamplerState>			m_default_sampler;

	D3D11_VIEWPORT			m_viewport = {};
	D3D11_RECT				m_scissor_rect = {};
	u32						m_frame_index = 0;

	Pool<COMPtr<ID3D11Buffer>, VertexBufferID>			m_vertex_buffers{ 128 };
	Pool<COMPtr<ID3D11Buffer>, ConstantBufferID>		m_constant_buffers{ 128 };
	Pool<ShaderResourceList, ShaderResourceListID>		m_shader_resource_lists{ 128 };
	Pool<InputAssemblerConfig, InputAssemblerConfigID>	m_ia_configs{ 128 };
	Pool<StreamFormat, StreamFormatID>					m_stream_formats{ 128 };
	Pool<Texture, TextureID>							m_textures{ 128 };
	Pool<VertexShader, VertexShaderID>					m_vertex_shaders{ 128 };
	Pool<COMPtr<ID3D11PixelShader>, PixelShaderID>		m_pixel_shaders{ 128 };
	Pool<LinkedProgram, ProgramID>						m_linked_programs{ 128 };

	// BEGIN GPUInterface functions
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void RecreateSwapChain(void* hwnd, u32 width, u32 height) override;

	virtual void BeginFrame() override;
	virtual void EndFrame() override;

	virtual void SubmitPass(const RenderPass& pass) override;

	virtual StreamFormatID RegisterStreamFormat(const GPU::StreamFormatDesc& format) override;
	virtual InputAssemblerConfigID RegisterInputAssemblyConfig(StreamFormatID format, mu::PointerRange<const VertexBufferID> vertex_buffers, IndexBufferID index_buffer) override;

	virtual VertexShaderID CompileVertexShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) override;
	virtual PixelShaderID CompilePixelShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) override;
	virtual ProgramID LinkProgram(VertexShaderID vertex_shader, PixelShaderID pixel_shader) override;

	virtual ConstantBufferID CreateConstantBuffer(mu::PointerRange<const u8> data) override;
	virtual VertexBufferID CreateVertexBuffer(mu::PointerRange<const u8> data) override;

	virtual TextureID CreateTexture2D(u32 width, u32 height, GPU::TextureFormat format, mu::PointerRange<const u8> data) override;

	virtual ShaderResourceListID CreateShaderResourceList(const GPU::ShaderResourceListDesc& desc) override;
	// END GPUInterface functions

	ID3D11RenderTargetView* GetRenderTargetView(RenderTargetID id) {
		CHECK(id == RenderTargetID{});
		return m_back_buffer_rtv;
	}
};

GPUInterface* CreateGPU_DX11() {
	return new GPU_DX11;
}


void GPU_DX11::Init() {
	UINT dxgiFactoryFlags = 0;

	{
		//COMPtr<ID3D12Debug> debug_interface;
		//if (SUCCEEDED(D3D11Debug(IID_PPV_ARGS(debug_interface.Replace())))) {
		//	debug_interface->EnableDebugLayer();
		//}
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}

	EnsureHR(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(m_dxgi_factory.Replace())));

	EnsureHR(D3D11CreateDevice(
		nullptr, // adapter
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr, // software module
		D3D11_CREATE_DEVICE_DEBUG, // flags
		nullptr, // feature levels
		0, // num feature levels
		D3D11_SDK_VERSION,
		m_device.Replace(),
		nullptr, // feature level
		m_immediate_context.Replace()
	));

	D3D11_SAMPLER_DESC sampler_desc = {};
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	EnsureHR(m_device->CreateSamplerState(&sampler_desc, m_default_sampler.Replace()));
}
void GPU_DX11::Shutdown() {
	m_device.Clear();
	m_immediate_context.Clear();
}
void GPU_DX11::RecreateSwapChain(void* hwnd, u32 width, u32 height) {
	m_viewport = D3D11_VIEWPORT{ 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	m_scissor_rect = D3D11_RECT{ 0, 0, (LONG)width, (LONG)height };

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
	swap_chain_desc.BufferCount = frame_count;
	swap_chain_desc.Width = width;
	swap_chain_desc.Height = height;
	swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.SampleDesc.Count = 1;
	
	{
		COMPtr<IDXGISwapChain1> swap_chain_tmp;
		m_dxgi_factory->CreateSwapChainForHwnd(
			m_device.Get(),
			(HWND)hwnd,
			&swap_chain_desc,
			nullptr, // fullscreen desc
			nullptr, // restrict output
			swap_chain_tmp.Replace());
		EnsureHR(m_dxgi_factory->MakeWindowAssociation((HWND)hwnd, DXGI_MWA_NO_ALT_ENTER));

		EnsureHR(swap_chain_tmp->QueryInterface(m_swap_chain.Replace()));
	}
	m_frame_index = m_swap_chain->GetCurrentBackBufferIndex();
	
	EnsureHR(m_swap_chain->GetBuffer(0, IID_PPV_ARGS(m_back_buffer.Replace())));

	D3D11_RENDER_TARGET_VIEW_DESC back_buffer_rtv_desc = {};
	back_buffer_rtv_desc.Format = swap_chain_desc.Format;
	back_buffer_rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	back_buffer_rtv_desc.Texture2D.MipSlice = 0;
	m_device->CreateRenderTargetView(m_back_buffer.Get(), &back_buffer_rtv_desc, m_back_buffer_rtv.Replace());
}
void GPU_DX11::BeginFrame() {
	float clear_color[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	m_immediate_context->ClearRenderTargetView(m_back_buffer_rtv.Get(), clear_color);
}
void GPU_DX11::EndFrame() {
	EnsureHR(m_swap_chain->Present(0, 0));
}
void GPU_DX11::SubmitPass(const RenderPass& pass) {
	COMPtr<ID3D11DeviceContext> context{ m_immediate_context };

	ID3D11RenderTargetView* rtvs[GPU::MaxRenderTargets];
	CHECK(pass.RenderTargets.Num() <= GPU::MaxRenderTargets);
	for (tuple<ID3D11RenderTargetView*&, RenderTargetID> it : Zip(rtvs, pass.RenderTargets)) {
		get<0>(it) = GetRenderTargetView(get<1>(it));
	}
	context->OMSetRenderTargets((u32)pass.RenderTargets.Num(), rtvs, nullptr);

	// TODO: Make part of render pass
	context->RSSetViewports(1, &m_viewport);
	context->RSSetScissorRects(1, &m_scissor_rect);

	for (const GPU::DrawItem& item : pass.DrawItems) {
		context->IASetPrimitiveTopology(DX::CommonToDX(item.Command.PrimTopology));

		const LinkedProgram& program = m_linked_programs[item.PipelineSetup.Program];
		context->VSSetShader(m_vertex_shaders[program.VertexShader].CompiledShader.Get(), nullptr, 0);
		context->PSSetShader(m_pixel_shaders[program.PixelShader].Get(), nullptr, 0);

		FixedArray<ID3D11Buffer*, GPU::MaxBoundConstantBuffers> cbs;
		for (ConstantBufferID cb_id : item.BoundResources.ConstantBuffers) {
			ID3D11Buffer* cb = m_constant_buffers[cb_id];
			cbs.Add(cb);
		}
		// TODO: Other stages if necessary, only linked stages?
		context->VSSetConstantBuffers(0, (u32)cbs.Num(), cbs.Data());
		context->PSSetConstantBuffers(0, (u32)cbs.Num(), cbs.Data());

		FixedArray<TextureID, GPU::MaxBoundShaderResources> bound_resources;
		bound_resources.AddMany(GPU::MaxBoundShaderResources, TextureID{ u32_max });
		for (ShaderResourceListID srl_id : item.BoundResources.ResourceLists) {
			const ShaderResourceList& list = m_shader_resource_lists[srl_id];
			FixedArray<ID3D11ShaderResourceView*, GPU::MaxBoundShaderResources> to_bind;
			for (TextureID tex_id : list.Desc.Textures) {
				to_bind.Add(m_textures[tex_id].SRV.Get());
			}
			context->VSSetShaderResources(list.Desc.StartSlot, (u32)to_bind.Num(), to_bind.Data());
			context->PSSetShaderResources(list.Desc.StartSlot, (u32)to_bind.Num(), to_bind.Data());
		}

		ID3D11SamplerState* samplers[] = { m_default_sampler };
		context->VSSetSamplers(0, 1, samplers);
		context->PSSetSamplers(0, 1, samplers);

		FixedArray<ID3D11Buffer*, GPU::MaxStreamSlots> vbs;
		const InputAssemblerConfig& ia_config = m_ia_configs[item.PipelineSetup.InputAssemblerConfig];
		const StreamFormat& stream_format = m_stream_formats[ia_config.StreamFormat];
		vbs.AddZeroed(ia_config.Slots.Num());
		for (tuple<ID3D11Buffer*&, VertexBufferID> vb_it : Zip(vbs, ia_config.Slots) ) {
			ID3D11Buffer*& vb = get<0>(vb_it);
			VertexBufferID vb_id = get<1>(vb_it);
			if (vb_id == VertexBufferID{}) {
				vb = nullptr;
			}
			else {
				vb = m_vertex_buffers[vb_id].Get();
			}
		}
		
		FixedArray<u32, GPU::MaxStreamSlots> offsets;
		offsets.AddZeroed(vbs.Num());
		context->IASetInputLayout(stream_format.InputLayout);
		context->IASetVertexBuffers(0, (u32)vbs.Num(), vbs.Data(), stream_format.Strides.Data(), offsets.Data()); // nullptr or all 0s?
		context->Draw(item.Command.VertexOrIndexCount, 0);
	}
}

StreamFormatID GPU_DX11::RegisterStreamFormat(const GPU::StreamFormatDesc& format) { 
	StreamFormatID id = m_stream_formats.AddDefaulted();
	StreamFormat& compiled_format = m_stream_formats[id];
	compiled_format.Desc = format;
	compiled_format.Strides.AddMany(format.Slots.Num(), 0);
	Array<D3D11_INPUT_ELEMENT_DESC> elems;
	for (u32 slot_idx = 0; slot_idx < format.Slots.Num(); ++slot_idx) {
		u32& stride = compiled_format.Strides[slot_idx];
		const GPU::StreamSlotDesc& slot = format.Slots[slot_idx];
		for (const GPU::StreamElementDesc& elem : slot.Elements) {
			stride += GPU::GetStreamElementSize(elem);
			elems.Add(D3D11_INPUT_ELEMENT_DESC{
				DX::GetSemanticName(elem.Semantic),
				elem.SemanticIndex,
				DX::GetStreamElementFormat(elem),
				slot_idx,
				D3D11_APPEND_ALIGNED_ELEMENT,
				D3D11_INPUT_PER_VERTEX_DATA,
				0
			});
		}
	}

	const char* shader_prefix =
		"struct vs_in {\n";
	const char* shader_suffix =
		"};\n"
		"float4 vs_main(vs_in in_vert) : SV_POSITION\n"
		"{ return float4(0,0,0,0); }\n";
	String shader = shader_prefix;
	u32 idx = 0;
	for (const GPU::StreamSlotDesc& slot : format.Slots) {
		for (const GPU::StreamElementDesc& elem : slot.Elements) {
			const char* type = DX::GetHLSLTypeName(elem);
			const char* semantic = DX::GetSemanticName(elem.Semantic);
			String elem_s = mu::Format(type, " ", "elem", idx, " : ", semantic, elem.SemanticIndex, ";\n");
			shader.Append(elem_s);
			++idx;
		}
	}
	shader.Append(shader_suffix);
	COMPtr<ID3D10Blob> compiled;
	DX::CompileShaderHLSL(compiled.Replace(), "vs_5_0", "vs_main", mu::Range((u8*)shader.GetRaw(), shader.GetLength()));

	EnsureHR(m_device->CreateInputLayout(elems.Data(), (u32)elems.Num(), compiled->GetBufferPointer(), compiled->GetBufferSize(), compiled_format.InputLayout.Replace()));

	return id;
}
InputAssemblerConfigID GPU_DX11::RegisterInputAssemblyConfig(StreamFormatID format, mu::PointerRange<const VertexBufferID> vertex_buffers, IndexBufferID index_buffer) {
	CHECK(vertex_buffers.Size() <= GPU::MaxStreamSlots);
	InputAssemblerConfigID id = m_ia_configs.AddDefaulted();
	InputAssemblerConfig& config = m_ia_configs[id];
	config.Slots.AddRange(vertex_buffers);
	config.IndexBuffer = index_buffer;
	config.StreamFormat = format;

	return id;
}

VertexShaderID GPU_DX11::CompileVertexShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) {
	VertexShaderID id = m_vertex_shaders.AddDefaulted();
	COMPtr<ID3DBlob> compiled_shader;
	DX::CompileShaderHLSL(compiled_shader.Replace(), "vs_5_0", entry_point, code);
	CHECK(compiled_shader.Get());
	
	EnsureHR(m_device->CreateVertexShader(compiled_shader->GetBufferPointer(), compiled_shader->GetBufferSize(), nullptr, m_vertex_shaders[id].CompiledShader.Replace()));
	VertexShaderInputs& inputs = m_vertex_shaders[id].Inputs;

	COMPtr<ID3D11ShaderReflection> reflector;
	EnsureHR(D3DReflect(
		compiled_shader->GetBufferPointer(),
		compiled_shader->GetBufferSize(),
		IID_PPV_ARGS(reflector.Replace())
	));
	D3D11_SHADER_DESC desc;
	EnsureHR(reflector->GetDesc(&desc));
	FixedArray<D3D11_SIGNATURE_PARAMETER_DESC, VertexShaderInputs::MaxInputElements> input_params;
	CHECK(desc.InputParameters < VertexShaderInputs::MaxInputElements);
	input_params.AddZeroed(desc.InputParameters);
	inputs.InputElements.AddZeroed(desc.InputParameters);
	for (tuple<u32, DX::VertexShaderInputElement&> it : Zip(Iota<u32>(), inputs.InputElements)) {
		D3D11_SIGNATURE_PARAMETER_DESC input_param;
		reflector->GetInputParameterDesc(get<0>(it), &input_param);

		get<1>(it) = DX11Util::ParseInputParameter(input_param);
	}

	return id;
}
PixelShaderID GPU_DX11::CompilePixelShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) {
	PixelShaderID id = m_pixel_shaders.AddDefaulted();
	COMPtr<ID3DBlob> compiled_shader;
	DX::CompileShaderHLSL(compiled_shader.Replace(), "ps_5_0", entry_point, code);
	CHECK(compiled_shader.Get());
	EnsureHR(m_device->CreatePixelShader(compiled_shader->GetBufferPointer(), compiled_shader->GetBufferSize(), nullptr, m_pixel_shaders[id].Replace()));
	return id;
}
ProgramID GPU_DX11::LinkProgram(VertexShaderID vertex_shader, PixelShaderID pixel_shader) { 
	ProgramID id = m_linked_programs.AddDefaulted();
	LinkedProgram& prog = m_linked_programs[id];
	prog.VertexShader = vertex_shader;
	prog.PixelShader = pixel_shader;
	return id;
}

ConstantBufferID GPU_DX11::CreateConstantBuffer(mu::PointerRange<const u8> data) { 
	ConstantBufferID id = m_constant_buffers.AddDefaulted();

	D3D11_BUFFER_DESC buffer_desc = {};
	buffer_desc.ByteWidth = (u32)data.Size();
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_SUBRESOURCE_DATA initial_data = { &data.Front(), 0, 0 };
	EnsureHR(m_device->CreateBuffer(&buffer_desc, &initial_data, m_constant_buffers[id].Replace()));
	return id;
}
VertexBufferID GPU_DX11::CreateVertexBuffer(mu::PointerRange<const u8> data) {
	VertexBufferID id = m_vertex_buffers.AddDefaulted();

	D3D11_BUFFER_DESC buffer_desc = {};
	buffer_desc.ByteWidth = (u32)data.Size();
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initial_data = { &data.Front(), 0, 0 };
	EnsureHR(m_device->CreateBuffer(&buffer_desc, &initial_data, m_vertex_buffers[id].Replace()));
	return id;
}

TextureID GPU_DX11::CreateTexture2D(u32 width, u32 height, GPU::TextureFormat format, mu::PointerRange<const u8> data) { 
	TextureID id = m_textures.AddDefaulted();
	Texture& texture = m_textures[id];

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DX::CommonToDX(format);
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA initial_data = { &data.Front(), DX::CalcRowPitch(format, width), DX::CalcRowPitch(format, width) };
	EnsureHR(m_device->CreateTexture2D(&desc, &initial_data, texture.Resource.Replace()));

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = { };
	srv_desc.Format = DX::CommonToDX(format);
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = 1;
	srv_desc.Texture2D.MostDetailedMip = 0;
	EnsureHR(m_device->CreateShaderResourceView(texture.Resource, &srv_desc, texture.SRV.Replace()));
	return id;
}

ShaderResourceListID GPU_DX11::CreateShaderResourceList(const GPU::ShaderResourceListDesc& desc) { 
	ShaderResourceListID id = m_shader_resource_lists.AddDefaulted();
	m_shader_resource_lists[id].Desc = desc;
	return id;
}

#pragma warning(pop)