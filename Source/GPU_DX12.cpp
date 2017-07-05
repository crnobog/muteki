// TOOD: unify case for var names by context

#include "mu-core/Debug.h"
#include "GPU_DX12.h"

#include "mu-core/Array.h"
#include "mu-core/FixedArray.h"
#include "mu-core/Pool.h"
#include "mu-core/Ranges.h"
#include "mu-core/Vectors.h"
#include "mu-core/Utils.h"

#include "GPU_DX12/Fence.h"
#include "GPU_DX12/Utils.h"

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DX12Util;
using std::tuple;
using std::get;
using namespace mu;

using StreamFormatID = GPUInterface::StreamFormatID;
using VertexBufferID = GPUInterface::VertexBufferID;
using IndexBufferID = GPUInterface::IndexBufferID;
using InputAssemblerConfigID = GPUInterface::InputAssemblerConfigID;
using VertexShaderID = GPUInterface::VertexShaderID;
using PixelShaderID = GPUInterface::PixelShaderID;
using ProgramID = GPUInterface::ProgramID;
using ConstantBufferID = GPUInterface::ConstantBufferID;
using RenderTargetID = GPUInterface::RenderTargetID;
using TextureID = GPUInterface::TextureID;
using ShaderResourceListID = GPUInterface::ShaderResourceListID;
using RenderPass = GPUInterface::RenderPass;

// Internal types
struct StreamFormat {
	u32 Strides[GPUInterface::MaxStreamSlots];
	GPUInterface::StreamFormatDesc Desc;
};

struct LinkedProgram {
	VertexShaderID VertexShader;
	PixelShaderID PixelShader;
};

struct VertexShaderInputs {
	static constexpr i32 MaxInputElements = 8;
	FixedArray<VertexShaderInputElement, MaxInputElements> InputElements;
};
struct VertexShader {
	COMPtr<ID3DBlob> CompiledShader;
	VertexShaderInputs Inputs;
};

struct InputAssemblerConfig {
	StreamFormatID StreamFormat;
	VertexBufferID Slots[GPUInterface::MaxStreamSlots];
	IndexBufferID IndexBuffer;
};

struct VertexBuffer {
	COMPtr<ID3D12Resource> Resource;
	u32 Size;

	D3D12_VERTEX_BUFFER_VIEW MakeVertexBufferView(u32 stride) {
		return { Resource->GetGPUVirtualAddress(), Size, stride };
	}
};

struct CachedPSO {
	GPUInterface::DrawPipelineSetup Setup;
	COMPtr<ID3D12PipelineState> PSO;
};

struct Texture {
	COMPtr<ID3D12Resource> Resource;
	COMPtr<ID3D12Resource> UploadResource;
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc;
};

struct ShaderResourceList {
	GPUInterface::ShaderResourceListDesc desc;
};

struct GPU_DX12 : public GPUInterface {
	static constexpr u32 frame_count = 2;
	static constexpr u32 max_descriptors_per_frame = 128; // TODO: Use a pool of descriptor heaps?
	static constexpr D3D12_HEAP_PROPERTIES upload_heap_properties{ D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
	static constexpr D3D12_HEAP_PROPERTIES default_heap_properties{ D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };

	COMPtr<IDXGIFactory4>				dxgi_factory;
	COMPtr<ID3D12Device>				device;
	COMPtr<ID3D12CommandQueue>			command_queue;
	COMPtr<ID3D12GraphicsCommandList>	command_list;
	COMPtr<ID3D12DescriptorHeap>		rtv_heap;
	COMPtr<ID3D12RootSignature>			root_signature;

	COMPtr<IDXGISwapChain3>				swap_chain;
	D3D12_VIEWPORT						viewport = {};
	D3D12_RECT							scissor_rect = {};

	Array<CachedPSO> cached_psos;

	COMPtr<ID3D12CommandQueue>			copy_command_queue;
	COMPtr<ID3D12GraphicsCommandList>	copy_command_list;
	COMPtr<ID3D12CommandAllocator>		copy_command_allocator;
	Fence								copy_fence;
	HANDLE								copy_fence_event = nullptr;

	u32 rtv_descriptor_size = 0;
	u32 srv_descriptor_size = 0;

	u32 frame_index = 0;

	struct FrameData {
		COMPtr<ID3D12Resource>			render_target;
		D3D12_CPU_DESCRIPTOR_HANDLE		render_target_view;
		COMPtr<ID3D12CommandAllocator>	command_allocator;
		COMPtr<ID3D12DescriptorHeap>	shader_resource_heap; // srv, cbv, uav
		u64								fence_value = 0;
	} frame_data[frame_count];

	Fence	frame_fence;
	HANDLE	frame_fence_event = nullptr;

	Pool<StreamFormat> stream_formats{ 128 };
	Pool<VertexShader> registered_vertex_shaders{ 128 };
	Pool<COMPtr<ID3DBlob>> registered_pixel_shaders{ 128 };
	Pool<LinkedProgram> linked_programs{ 128 };
	Pool<VertexBuffer> vertex_buffers{ 128 };
	Pool<InputAssemblerConfig> ia_configs{ 128 };
	Pool<COMPtr<ID3D12Resource>> constant_buffers{ 128 };
	Pool<Texture> textures{ 128 };
	Pool<ShaderResourceList> shader_resource_lists{ 128 };

	// BEGIN GPUInterface functions
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void RecreateSwapChain(void* hwnd, u32 width, u32 height) override;
	
	virtual void BeginFrame() override;
	virtual void EndFrame() override;
	
	virtual void SubmitPass(const RenderPass& pass) override;
	
	virtual StreamFormatID RegisterStreamFormat(const GPUInterface::StreamFormatDesc& format) override;
	virtual InputAssemblerConfigID RegisterInputAssemblyConfig(StreamFormatID format, mu::PointerRange<const VertexBufferID> vertex_buffers, IndexBufferID index_buffer) override;
	
	virtual VertexShaderID CompileVertexShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) override;
	virtual PixelShaderID CompilePixelShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) override;
	virtual ProgramID LinkProgram(VertexShaderID vertex_shader, PixelShaderID pixel_shader) override;
	
	virtual ConstantBufferID CreateConstantBuffer(mu::PointerRange<const u8> data) override;
	virtual VertexBufferID CreateVertexBuffer(mu::PointerRange<const u8> data) override;
	
	virtual TextureID CreateTexture2D(u32 width, u32 height, GPUInterface::TextureFormat format, mu::PointerRange<const u8> data) override;

	virtual ShaderResourceListID CreateShaderResourceList(const GPUInterface::ShaderResourceListDesc& desc) override;
	// END GPUInterface functions


	ID3D12PipelineState* CachePSO(const GPUInterface::DrawPipelineSetup& pipeline_setup);
	D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE backbuffer, RenderTargetID id);
};

GPUInterface* CreateGPU_DX12() {
	return new GPU_DX12;
}

void GPU_DX12::Init() {
	UINT dxgiFactoryFlags = 0;

	{
		COMPtr<ID3D12Debug> debug_interface;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debug_interface.Replace())))) {
			debug_interface->EnableDebugLayer();
		}
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}

	EnsureHR(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(dxgi_factory.Replace())));
	EnsureHR(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.Replace())));

	{
		D3D12_COMMAND_QUEUE_DESC queue_desc = {};
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heap_desc.NumDescriptors = max_descriptors_per_frame;
		heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		EnsureHR(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(command_queue.Replace())));
		for (u32 i = 0; i < frame_count; ++i) {
			EnsureHR(device->CreateCommandAllocator(queue_desc.Type, IID_PPV_ARGS(frame_data[i].command_allocator.Replace())));
			EnsureHR(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(frame_data[i].shader_resource_heap.Replace())));
		}
	}

	EnsureHR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(frame_fence.fence.Replace())));
	EnsureHR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(copy_fence.fence.Replace())));

	EnsureHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame_data[0].command_allocator.Get(), nullptr, IID_PPV_ARGS(command_list.Replace())));
	command_list->Close(); // Force closed so it doesn't hold on to command allocator 0

	D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
	rtv_heap_desc.NumDescriptors = frame_count;
	rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	EnsureHR(device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(rtv_heap.Replace())));

	rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(rtv_heap_desc.Type);
	srv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	frame_fence_event = CreateEvent(nullptr, false, false, L"Frame Fence Event");
	copy_fence_event = CreateEvent(nullptr, false, false, L"Upload Fence Event");

	{
		D3D12_COMMAND_QUEUE_DESC queue_desc = {};
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

		EnsureHR(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(copy_command_queue.Replace())));
	}
	EnsureHR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(copy_command_allocator.Replace())));
	EnsureHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, copy_command_allocator.Get(), nullptr, IID_PPV_ARGS(copy_command_list.Replace())));

	EnsureHR(copy_command_list->Close());

	// Empty root signature
	{
		Array<D3D12_ROOT_PARAMETER> root_parameters;

		{
			auto r = root_parameters.AddZeroed(2);
			D3D12_ROOT_PARAMETER* param = &r.Front();
			param->ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			param->ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			param->Descriptor.ShaderRegister = 0;
			param->Descriptor.RegisterSpace = 0;

			r.Advance(); param = &r.Front();
			static const D3D12_DESCRIPTOR_RANGE table = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, GPUInterface::MaxBoundShaderResources, 0, 0, 0 }; // Make sure pointer is valid outside this block
			param->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param->ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			param->DescriptorTable.NumDescriptorRanges = 1;
			param->DescriptorTable.pDescriptorRanges = &table;
		}
		Array<D3D12_STATIC_SAMPLER_DESC> static_samplers;
		static_samplers.Add({
			D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			0,
			0,
			D3D12_COMPARISON_FUNC_NEVER,
			D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
			0.0f,
			D3D12_FLOAT32_MAX,
			0,
			0,
			D3D12_SHADER_VISIBILITY_ALL,
		});
		D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {
			(u32)root_parameters.Num(), root_parameters.Data(),
			(u32)static_samplers.Num(), static_samplers.Data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
		COMPtr<ID3DBlob> signature_blob, error_blob;
		EnsureHR(D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, signature_blob.Replace(), error_blob.Replace()));
		EnsureHR(device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(), IID_PPV_ARGS(root_signature.Replace())));
	}
}

void GPU_DX12::Shutdown() {
	for (i32 i = 0; i < frame_count; ++i) {
		auto& frame = frame_data[i];

		frame_fence.WaitForFence(frame.fence_value, frame_fence_event);
	}

	stream_formats.Empty();
	registered_vertex_shaders.Empty();
}

void GPU_DX12::RecreateSwapChain(void* hwnd, u32 width, u32 height) {
	viewport = D3D12_VIEWPORT{ 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	scissor_rect = D3D12_RECT{ 0, 0, (LONG)width, (LONG)height };

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
		EnsureHR(dxgi_factory->CreateSwapChainForHwnd(
			command_queue.Get(),
			(HWND)hwnd,
			&swap_chain_desc,
			nullptr, // fullscreen desc
			nullptr, // restrict output
			swap_chain_tmp.Replace()));
		EnsureHR(dxgi_factory->MakeWindowAssociation((HWND)hwnd, DXGI_MWA_NO_ALT_ENTER));

		EnsureHR(swap_chain_tmp->QueryInterface(swap_chain.Replace()));
	}
	frame_index = swap_chain->GetCurrentBackBufferIndex();

	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
	for (u32 n = 0; n < frame_count; ++n) {
		EnsureHR(swap_chain->GetBuffer(n, IID_PPV_ARGS(frame_data[n].render_target.Replace())));
		device->CreateRenderTargetView(frame_data[n].render_target.Get(), nullptr, rtv_handle);
		frame_data[n].render_target_view = rtv_handle;
		rtv_handle.ptr += rtv_descriptor_size;
	}
}

void GPU_DX12::BeginFrame() {
	auto& frame = frame_data[frame_index];

	frame_fence.WaitForFence(frame.fence_value, frame_fence_event);

	EnsureHR(frame.command_allocator->Reset());

	EnsureHR(command_list->Reset(frame.command_allocator.Get(), nullptr));

	command_list->RSSetViewports(1, &viewport);
	command_list->RSSetScissorRects(1, &scissor_rect);

	auto present_to_rt = ResourceBarrier{ frame.render_target.Get(), ResourceState::Present, ResourceState::RenderTarget };
	command_list->ResourceBarrier(1, &present_to_rt);

	command_list->OMSetRenderTargets(1, &frame.render_target_view, false, nullptr);
	float clear_color[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	command_list->ClearRenderTargetView(frame.render_target_view, clear_color, 0, nullptr);

	EnsureHR(command_list->Close());

	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(1, command_lists);
}

void GPU_DX12::EndFrame() {
	auto& frame = frame_data[frame_index];
	EnsureHR(command_list->Reset(frame.command_allocator.Get(), nullptr));

	auto rt_to_present = ResourceBarrier{ frame.render_target.Get(), ResourceState::RenderTarget, ResourceState::Present };
	command_list->ResourceBarrier(1, &rt_to_present);

	EnsureHR(command_list->Close());

	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(1, command_lists);

	EnsureHR(swap_chain->Present(1, 0));

	// Update fence value
	frame.fence_value = frame_fence.SubmitFence(command_queue.Get());
	frame_index = swap_chain->GetCurrentBackBufferIndex();
}

StreamFormatID GPU_DX12::RegisterStreamFormat(const GPUInterface::StreamFormatDesc& format) {
	StreamFormatID id{ (u32)stream_formats.AddDefaulted() };
	StreamFormat& compiled_format = stream_formats[id.Index];
	compiled_format.Desc = format;
	for (tuple<const GPUInterface::StreamSlotDesc&, u32&> it : Zip(format.Slots, compiled_format.Strides)) {
		u32& stride = get<1>(it);
		stride = 0;
		const GPUInterface::StreamSlotDesc& slot = get<0>(it);
		for (GPUInterface::StreamElementDesc& elem : slot.Elements) {
			stride += GPUInterface::GetStreamElementSize(elem);
		}
	}

	return id;
}




VertexShaderID GPU_DX12::CompileVertexShaderHLSL(const char* entry_point, PointerRange<const u8> code) {
	VertexShaderID id{ (u32)registered_vertex_shaders.AddDefaulted() };
	COMPtr<ID3DBlob>& compiled_shader = registered_vertex_shaders[id.Index].CompiledShader;
	CompileShaderHLSL(compiled_shader.Replace(), "vs_5_0", entry_point, code);
	CHECK(compiled_shader.Get());

	VertexShaderInputs& inputs = registered_vertex_shaders[id.Index].Inputs;

	COMPtr<ID3D12ShaderReflection> reflector;
	EnsureHR(D3DReflect(
		compiled_shader->GetBufferPointer(),
		compiled_shader->GetBufferSize(),
		IID_PPV_ARGS(reflector.Replace())
	));
	D3D12_SHADER_DESC desc;
	EnsureHR(reflector->GetDesc(&desc));
	Array<D3D12_SIGNATURE_PARAMETER_DESC> input_params;
	CHECK(desc.InputParameters < VertexShaderInputs::MaxInputElements);
	input_params.AddZeroed(desc.InputParameters);
	inputs.InputElements.AddZeroed(desc.InputParameters);
	for (tuple<u32, VertexShaderInputElement&> it : Zip(Iota<u32>(), inputs.InputElements)) {
		D3D12_SIGNATURE_PARAMETER_DESC input_param;
		reflector->GetInputParameterDesc(get<0>(it), &input_param);

		get<1>(it) = ParseInputParameter(input_param);
	}

	return id;
}
PixelShaderID GPU_DX12::CompilePixelShaderHLSL(const char* entry_point, PointerRange<const u8> code) {
	PixelShaderID id{ (u32)registered_pixel_shaders.AddDefaulted() };
	ID3DBlob** compiled_shader = registered_pixel_shaders[id.Index].Replace();
	CompileShaderHLSL(compiled_shader, "ps_5_0", entry_point, code);
	CHECK(*compiled_shader);
	return id;
}

ProgramID GPU_DX12::LinkProgram(VertexShaderID vertex_shader, PixelShaderID pixel_shader) {
	ProgramID id{ (u32)linked_programs.AddDefaulted() };
	linked_programs[id.Index] = { vertex_shader, pixel_shader };
	return id;
}

ConstantBufferID GPU_DX12::CreateConstantBuffer(PointerRange<const u8> data) {
	ConstantBufferID id{ (u32)constant_buffers.AddDefaulted() };

	auto cbuffer_desc = BufferDesc{ data.Size() };
	EnsureHR(device->CreateCommittedResource(
		&upload_heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&cbuffer_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(constant_buffers[id.Index].Replace())
	));
	void* mapped = nullptr;
	D3D12_RANGE read_range{ 0, 0 };
	EnsureHR(constant_buffers[id.Index]->Map(0, &read_range, &mapped));
	memcpy(mapped, &data.Front(), data.Size());
	constant_buffers[id.Index]->Unmap(0, nullptr);

	return id;
}

VertexBufferID GPU_DX12::CreateVertexBuffer(PointerRange<const u8> data) {
	VertexBufferID id{ (u32)vertex_buffers.AddDefaulted() };
	CHECK(vertex_buffers[id.Index].Resource.Get() == nullptr);
	vertex_buffers[id.Index].Size = (u32)data.Size();

	auto vbuffer_desc = BufferDesc{ (u32)data.Size() };
	EnsureHR(device->CreateCommittedResource(
		&upload_heap_properties, // TODO: Upload & copy properly
		D3D12_HEAP_FLAG_NONE,
		&vbuffer_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(vertex_buffers[id.Index].Resource.Replace()))
	);

	D3D12_RANGE read_range{ 0, 0 };
	void* mapped = nullptr;
	EnsureHR(vertex_buffers[id.Index].Resource->Map(0, &read_range, &(void*)mapped));
	memcpy(mapped, &data.Front(), data.Size());
	vertex_buffers[id.Index].Resource->Unmap(0, nullptr);

	return id;
}

TextureID GPU_DX12::CreateTexture2D(u32 width, u32 height, GPUInterface::TextureFormat format, PointerRange<const u8> data) {
	TextureID id{ (u32)textures.AddDefaulted() };
	Texture& texture = textures[id.Index];

	TextureDesc2D desc{ width, height, CommonToDX12(format) };
	EnsureHR(device->CreateCommittedResource(
		&default_heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(texture.Resource.Replace())
	));

	texture.SRVDesc = TextureViewDesc2D{ CommonToDX12(format) };

	u32 num_rows = 0;
	u64 row_size = 0, total_size = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
	device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, &num_rows, &row_size, &total_size);

	BufferDesc upload_desc{ total_size };
	EnsureHR(device->CreateCommittedResource(
		&upload_heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&upload_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(texture.UploadResource.Replace())
	));

	{
		void* mapped = nullptr;
		texture.UploadResource->Map(0, nullptr, &mapped);
		D3D12_MEMCPY_DEST dest{ mapped, layout.Footprint.RowPitch, layout.Footprint.RowPitch * num_rows };
		D3D12_SUBRESOURCE_DATA source{ &data.Front(), CalcRowPitch(format, width), CalcRowPitch(format, width) * 1 };
		MemcpySubresource(&dest, &source, row_size, num_rows, layout.Footprint.Depth);
		texture.UploadResource->Unmap(0, nullptr);
	}

	EnsureHR(copy_command_list->Reset(copy_command_allocator.Get(), nullptr));

	auto copy_dest_to_shader_resource = ResourceBarrier{ texture.Resource.Get(), ResourceState::CopyDest, ResourceState::Common };
	copy_command_list->ResourceBarrier(1, &copy_dest_to_shader_resource);

	TextureCopyLocation copy_src{ texture.UploadResource.Get(), layout };
	TextureCopyLocation copy_dest{ texture.Resource.Get(), 0 };
	copy_command_list->CopyTextureRegion(&copy_dest, 0, 0, 0, &copy_src, nullptr);

	EnsureHR(copy_command_list->Close());
	ID3D12CommandList* command_lists[] = { copy_command_list.Get() };
	copy_command_queue->ExecuteCommandLists(1, command_lists);
	u64 fence_value = copy_fence.SubmitFence(copy_command_queue.Get());
	copy_fence.WaitForFence(fence_value, copy_fence_event);

	return id;
}

ShaderResourceListID GPU_DX12::CreateShaderResourceList(const GPUInterface::ShaderResourceListDesc& desc) {
	ShaderResourceListID id{ (u32)shader_resource_lists.AddDefaulted() };
	ShaderResourceList& srl = shader_resource_lists[id.Index];
	srl.desc = desc;

	return id;
}

InputAssemblerConfigID GPU_DX12::RegisterInputAssemblyConfig(StreamFormatID stream_id, PointerRange<const VertexBufferID> vbuffers, IndexBufferID ibuffer) {
	CHECK(vbuffers.Size() <= GPUInterface::MaxStreamSlots);
	InputAssemblerConfigID id{ (u32)ia_configs.AddDefaulted() };
	InputAssemblerConfig& config = ia_configs[id.Index];
	Copy(Range(config.Slots), vbuffers);
	config.IndexBuffer = ibuffer;
	config.StreamFormat = stream_id;

	return id;
}

// TODO: don't-care states?
bool operator==(const GPUInterface::DrawPipelineSetup& a, const GPUInterface::DrawPipelineSetup& b) {
	return a.BlendState.Index == b.BlendState.Index
		&& a.DepthStencilState.Index == b.DepthStencilState.Index
		&& a.InputAssemblerConfig.Index == b.InputAssemblerConfig.Index
		&& a.Program.Index == b.Program.Index
		&& a.RasterState.Index == b.RasterState.Index;
}

ID3D12PipelineState* GPU_DX12::CachePSO(const GPUInterface::DrawPipelineSetup& pipeline_setup) {
	auto found = Find(cached_psos, [&](CachedPSO& cached) {
		if (cached.Setup == pipeline_setup) {
			return true;
		}
		return false;
	});
	if (!found.IsEmpty()) {
		return found.Front().PSO;
	}

	size_t index = cached_psos.Add({ pipeline_setup, COMPtr<ID3D12PipelineState>{} });
	COMPtr<ID3D12PipelineState>& new_pipeline_state = cached_psos[index].PSO;
	LinkedProgram program = linked_programs[pipeline_setup.Program.Index];

	// TODO
	D3D12_INPUT_ELEMENT_DESC input_layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	auto pipeline_desc = GraphicsPipelineStateDesc{ root_signature.Get() }
		.VS(registered_vertex_shaders[program.VertexShader.Index].CompiledShader)
		.PS(registered_pixel_shaders[program.PixelShader.Index])
		.DepthEnable(true)
		.PrimType(PrimType::Triangle)
		.RenderTargets(DXGI_FORMAT_R8G8B8A8_UNORM)
		.InputLayout(input_layout, (u32)ArraySize(input_layout));
	EnsureHR(device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(new_pipeline_state.Replace())));

	return new_pipeline_state.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE GPU_DX12::GetRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE backbuffer, RenderTargetID id) {
	CHECK(id.Index == u32_max);
	return backbuffer; // TODO
}

void GPU_DX12::SubmitPass(const GPUInterface::RenderPass& pass) {
	// Create required PSOs for all draw items
	Array<ID3D12PipelineState*> item_psos;
	item_psos.Reserve(pass.DrawItems.Size());

	for (const GPUInterface::DrawItem& item : pass.DrawItems) {
		ID3D12PipelineState* pso = CachePSO(item.PipelineSetup);
		CHECK(pso);
		item_psos.Add(pso);
	}

	auto& frame = frame_data[frame_index];
	EnsureHR(command_list->Reset(frame.command_allocator.Get(), nullptr));
	command_list->SetGraphicsRootSignature(root_signature.Get());

	ID3D12DescriptorHeap* heaps[] = { frame.shader_resource_heap.Get() };
	command_list->SetDescriptorHeaps((u32)ArraySize(heaps), heaps);

	// Bind RTs for pass
	CHECK(pass.RenderTargets.Num() <= GPUInterface::MaxRenderTargets);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[GPUInterface::MaxRenderTargets];
	for (tuple<D3D12_CPU_DESCRIPTOR_HANDLE&, RenderTargetID> it : Zip(rtvs, pass.RenderTargets)) {
		get<0>(it) = GetRenderTargetView(frame.render_target_view, get<1>(it));
	}
	command_list->OMSetRenderTargets((u32)pass.RenderTargets.Num(), rtvs, false, nullptr);

	// TODO: Make part of render pass
	command_list->RSSetViewports(1, &viewport);
	command_list->RSSetScissorRects(1, &scissor_rect);

	D3D12_CPU_DESCRIPTOR_HANDLE srv_heap_cursor{ frame.shader_resource_heap->GetCPUDescriptorHandleForHeapStart() };
	D3D12_GPU_DESCRIPTOR_HANDLE srv_heap_cursor_gpu{ frame.shader_resource_heap->GetGPUDescriptorHandleForHeapStart() };

	// Translate draw commands into CommandList calls
	for (tuple<ID3D12PipelineState*, GPUInterface::DrawItem> draw_it : Zip(item_psos, pass.DrawItems)) {
		ID3D12PipelineState* pso = get<0>(draw_it);
		const GPUInterface::DrawItem& item = get<1>(draw_it);

		command_list->SetPipelineState(pso);
		command_list->IASetPrimitiveTopology(CommonToDX12(item.Command.PrimTopology));

		CHECK(item.BoundResources.ConstantBuffers.Num() <= 1);
		if (item.BoundResources.ConstantBuffers.Num() > 0 && item.BoundResources.ConstantBuffers[0].Index != u32_max) {
			command_list->SetGraphicsRootConstantBufferView(0, constant_buffers[item.BoundResources.ConstantBuffers[0].Index]->GetGPUVirtualAddress());
		}

		FixedArray<TextureID, GPUInterface::MaxBoundShaderResources> bound_resources;
		bound_resources.AddMany(GPUInterface::MaxBoundShaderResources, TextureID{ u32_max });
		for (ShaderResourceListID id : item.BoundResources.ResourceLists) {
			const ShaderResourceList& list = shader_resource_lists[id.Index];
			for (u32 i = list.desc.StartSlot; i < list.desc.Textures.Num() && (i + list.desc.StartSlot < GPUInterface::MaxBoundShaderResources); ++i) {
				bound_resources[i] = list.desc.Textures[i - list.desc.StartSlot];
			}
		}

		D3D12_GPU_DESCRIPTOR_HANDLE table_root = srv_heap_cursor_gpu;
		for (u32 i = 0; i < bound_resources.Num(); ++i) {
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor_dest = srv_heap_cursor;
			srv_heap_cursor.ptr += srv_descriptor_size;
			srv_heap_cursor_gpu.ptr += srv_descriptor_size;
			TextureID id = bound_resources[i];
			if (id.Index == u32_max) { continue; }
			auto& texture = textures[id.Index];
			device->CreateShaderResourceView(texture.Resource.Get(), &texture.SRVDesc, descriptor_dest);
		}

		command_list->SetGraphicsRootDescriptorTable(1, table_root);

		D3D12_VERTEX_BUFFER_VIEW vbs[GPUInterface::MaxStreamSlots];
		//D3D12_INDEX_BUFFER_VIEW ib;
		const InputAssemblerConfig& ia_config = ia_configs[item.PipelineSetup.InputAssemblerConfig.Index];
		const StreamFormat& stream_format = stream_formats[ia_config.StreamFormat.Index];
		for (tuple<D3D12_VERTEX_BUFFER_VIEW&, VertexBufferID, u32> vb_it : Zip(vbs, ia_config.Slots, stream_format.Strides)) {
			D3D12_VERTEX_BUFFER_VIEW& view = get<0>(vb_it);
			VertexBufferID vb_id = get<1>(vb_it);
			if (vb_id.Index == u32_max) {
				view = { 0, 0, 0 };
			}
			else {

				u32 stride = get<2>(vb_it);
				view = vertex_buffers[vb_id.Index].MakeVertexBufferView(stride);
			}
		}

		command_list->IASetVertexBuffers(0, (u32)ArraySize(vbs), vbs);
		//if (ia_config.IndexBuffer.Index != u32_max) {
		//	ib = GetIndexBufferView(ia_config.IndexBuffer);
		//	command_list->IASetIndexBuffer(&ib);
		//}
		command_list->DrawInstanced(item.Command.VertexOrIndexCount, 1, 0, 0);
	}

	EnsureHR(command_list->Close());

	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(1, command_lists);
}
