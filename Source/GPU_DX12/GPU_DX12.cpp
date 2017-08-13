// TOOD: unify case for var names by context

#include "mu-core/Debug.h"
#include "GPU_DX12.h"

#include "mu-core/Array.h"
#include "mu-core/FixedArray.h"
#include "mu-core/Pool.h"
#include "mu-core/Ranges.h"
#include "mu-core/Utils.h"

#include "Fence.h"
#include "DX12Utils.h"
#include "../Vectors.h"

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
using DX::COMPtr;

using VertexBufferID = GPU::VertexBufferID;
using IndexBufferID = GPU::IndexBufferID;
using VertexShaderID = GPU::VertexShaderID;
using PixelShaderID = GPU::PixelShaderID;
using ProgramID = GPU::ProgramID;
using ConstantBufferID = GPU::ConstantBufferID;
using RenderTargetID = GPU::RenderTargetID;
using TextureID = GPU::TextureID;
using ShaderResourceListID = GPU::ShaderResourceListID;
using RenderPass = GPU::RenderPass;
using PipelineStateID = GPU::PipelineStateID;

struct GPUAllocation {
	D3D12_GPU_VIRTUAL_ADDRESS	GPUAddress;
	u32							Size;
	void*						CPUAddress;
};

struct DX12LinearAllocator {
	struct GPU_DX12* m_gpu = nullptr;
	COMPtr<ID3D12Resource> m_data;
	u8* m_data_cpu_ptr = nullptr;
	size_t m_max = 0;
	size_t m_used = 0;

	DX12LinearAllocator(struct GPU_DX12* gpu);
	GPUAllocation Allocate(u32 size);
	void FrameReset() {
		m_used = 0;
	}
};

struct DX12DescriptorTable {
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpu_handle;
	D3D12_GPU_DESCRIPTOR_HANDLE m_gpu_handle;
	u32 m_descriptor_size;
	u32 m_num_descriptors;

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(u32 index) { 
		return { m_cpu_handle.ptr + index * m_descriptor_size };
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(u32 index) {
		return { m_gpu_handle.ptr + index * m_descriptor_size };
	}
};

struct DX12DescriptorTableLinearAllocator {
	struct GPU_DX12* m_gpu = nullptr;
	COMPtr<ID3D12DescriptorHeap> m_heap;
	u32 m_total_descriptors;
	u32 m_used_descriptors;
	u32 m_descriptor_size;

	DX12DescriptorTableLinearAllocator(GPU_DX12* gpu);

	DX12DescriptorTable AllocateTable(u32 num_descriptors);
	void FrameReset() {
		m_used_descriptors = 0;
	}
};

struct GPU_DX12_Frame : public GPUFrameInterface {
	struct GPU_DX12* m_gpu = nullptr;
	GPU_DX12_Frame(struct GPU_DX12* gpu) 
		: m_gpu(gpu)
		, linear_allocator(gpu)
		, descriptor_table_allocator(gpu) {
	}

	COMPtr<ID3D12Resource>					render_target;
	D3D12_CPU_DESCRIPTOR_HANDLE				render_target_view;
	u64										fence_value = 0;
	DX12DescriptorTableLinearAllocator		descriptor_table_allocator;
	DX12LinearAllocator						linear_allocator;
	Array<D3D12_CONSTANT_BUFFER_VIEW_DESC>	temp_cbs;
	Array<D3D12_VERTEX_BUFFER_VIEW>			temp_vbs;	// stride is not stored
	Array<D3D12_INDEX_BUFFER_VIEW>			temp_ibs;	// format is not stored

	Array<COMPtr<ID3D12CommandAllocator>>	command_allocators;
	size_t									used_command_allocators = 0;

	virtual ConstantBufferID GetTemporaryConstantBuffer(PointerRange<const u8>) override;
	virtual VertexBufferID GetTemporaryVertexBuffer(mu::PointerRange<const u8> data) override;
	virtual IndexBufferID GetTemporaryIndexBuffer(mu::PointerRange<const u8> data) override;

	ID3D12CommandAllocator*	GetCommandAllocator();
};

struct GPU_DX12 : public GPUInterface {

	// Internal types
	struct StreamFormat {
		FixedArray<u32, GPU::MaxStreamSlots> Strides;
		GPU::StreamFormatDesc Desc;
		Array<D3D12_INPUT_ELEMENT_DESC> InputLayout;
	};

	struct LinkedProgram {
		VertexShaderID VertexShader;
		PixelShaderID PixelShader;
	};

	struct VertexShaderInputs {
		static constexpr i32 MaxInputElements = 8;
		FixedArray<DX::VertexShaderInputElement, MaxInputElements> InputElements;
	};
	struct VertexShader {
		COMPtr<ID3DBlob> CompiledShader;
		VertexShaderInputs Inputs;
	};

	struct InputAssemblerConfig {
		FixedArray<VertexBufferID, GPU::MaxStreamSlots> Slots;
		IndexBufferID IndexBuffer;
	};

	struct VertexBuffer {
		COMPtr<ID3D12Resource> Resource;
		u32 Size;

		D3D12_VERTEX_BUFFER_VIEW MakeVertexBufferView(u32 stride) {
			return { Resource->GetGPUVirtualAddress(), Size, stride };
		}
	};

	struct IndexBuffer {
		COMPtr<ID3D12Resource> Resource;
		u32 Size;

		D3D12_INDEX_BUFFER_VIEW MakeIndexBufferView(DXGI_FORMAT format) {
			return { Resource->GetGPUVirtualAddress(), Size, format };
		}
	};

	struct PipelineState {
		GPU::PipelineStateDesc					Desc;
		COMPtr<ID3D12PipelineState>				PSO;
		FixedArray<u32, GPU::MaxStreamSlots>	VBStrides;
	};

	struct Texture {
		COMPtr<ID3D12Resource> Resource;
		COMPtr<ID3D12Resource> UploadResource;
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	};

	struct ShaderResourceList {
		GPU::ShaderResourceListDesc Desc;
	};

	struct ConstantBuffer {
		COMPtr<ID3D12Resource> Resource;
		D3D12_CONSTANT_BUFFER_VIEW_DESC ViewDesc = { 0, 0 };
	};

	GPU_DX12();
	~GPU_DX12();
	GPU_DX12(const GPU_DX12&) = delete;
	void operator=(const GPU_DX12&) = delete;
	GPU_DX12(GPU_DX12&&) = delete;
	void operator=(GPU_DX12&&) = delete;

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
	Vector<u32, 2>						swap_chain_dimensions;
	
	COMPtr<ID3D12CommandQueue>			copy_command_queue;
	COMPtr<ID3D12GraphicsCommandList>	copy_command_list;
	COMPtr<ID3D12CommandAllocator>		copy_command_allocator;
	Fence								copy_fence;
	HANDLE								copy_fence_event = nullptr;

	u32 rtv_descriptor_size = 0;
	u32 srv_descriptor_size = 0;

	u32 frame_index = 0;

	GPU_DX12_Frame* frame_data[frame_count] = { nullptr, };

	Fence	frame_fence;
	HANDLE	frame_fence_event = nullptr;

	Pool<VertexShader,				VertexShaderID> registered_vertex_shaders{ 128 };
	Pool<COMPtr<ID3DBlob>,			PixelShaderID> registered_pixel_shaders{ 128 };
	Pool<LinkedProgram,				ProgramID> linked_programs{ 128 };
	Pool<Texture,					TextureID> textures{ 128 };
	Pool<ShaderResourceList,		ShaderResourceListID> shader_resource_lists{ 128 };
	Pool<PipelineState,				PipelineStateID> pipeline_states{ 128 };

	static constexpr size_t max_persistent_cbs = 128;
	Pool<ConstantBuffer, ConstantBufferID> constant_buffers{ max_persistent_cbs };
	static constexpr size_t max_persistent_vbs = 128;
	Pool<VertexBuffer, VertexBufferID> vertex_buffers{ max_persistent_vbs};
	static constexpr size_t max_persistent_ibs = 128;
	Pool<IndexBuffer, IndexBufferID> index_buffers{ max_persistent_ibs };

	// BEGIN GPUInterface functions
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void CreateSwapChain(void* hwnd, u32 width, u32 height) override;
	virtual void ResizeSwapChain(void* hwnd, u32 width, u32 height) override;
	virtual Vector<u32, 2> GetSwapChainDimensions() override {
		return swap_chain_dimensions;
	}

	virtual GPUFrameInterface* BeginFrame() override;
	virtual void EndFrame(GPUFrameInterface* frame) override;

	virtual void SubmitPass(const RenderPass& pass) override;

	virtual VertexShaderID CompileVertexShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) override;
	virtual PixelShaderID CompilePixelShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) override;
	virtual ProgramID LinkProgram(VertexShaderID vertex_shader, PixelShaderID pixel_shader) override;

	virtual GPU::PipelineStateID CreatePipelineState(const GPU::PipelineStateDesc& desc) override;
	virtual void DestroyPipelineState(GPU::PipelineStateID id) override;

	virtual ConstantBufferID CreateConstantBuffer(mu::PointerRange<const u8> data) override;
	virtual void DestroyConstantBuffer(ConstantBufferID id) override;

	virtual VertexBufferID CreateVertexBuffer(mu::PointerRange<const u8> data) override;
	virtual void DestroyVertexBuffer(GPU::VertexBufferID id) override;
	virtual IndexBufferID CreateIndexBuffer(mu::PointerRange<const u8> data) override;
	virtual void DestroyIndexBuffer(GPU::IndexBufferID id) override;

	virtual TextureID CreateTexture2D(u32 width, u32 height, GPU::TextureFormat format, mu::PointerRange<const u8> data) override;

	virtual ShaderResourceListID CreateShaderResourceList(const GPU::ShaderResourceListDesc& desc) override;
	// END GPUInterface functions

	void OnSwapChainUpdated();

	D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE backbuffer, RenderTargetID id);
	D3D12_CONSTANT_BUFFER_VIEW_DESC GetConstantBufferViewDesc(GPU_DX12_Frame* frame, ConstantBufferID id);
	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView(GPU_DX12_Frame* frame, VertexBufferID vb_id, u32 stride);
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView(GPU_DX12_Frame* frame, IndexBufferID id);
};

GPU_DX12::GPU_DX12() {
}

GPU_DX12::~GPU_DX12() {
	for (u32 i = 0; i < frame_count; ++i) {
		delete frame_data[i];
	}
}

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

	for (u32 i = 0; i < frame_count; ++i) {
		frame_data[i] = new GPU_DX12_Frame(this);
	}

	{
		D3D12_COMMAND_QUEUE_DESC queue_desc = {};
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		EnsureHR(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(command_queue.Replace())));
		for (u32 i = 0; i < frame_count; ++i) {
			frame_data[i]->command_allocators.Add({});
			EnsureHR(device->CreateCommandAllocator(queue_desc.Type, IID_PPV_ARGS(frame_data[i]->command_allocators[0].Replace())));
		}
	}

	EnsureHR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(frame_fence.fence.Replace())));
	EnsureHR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(copy_fence.fence.Replace())));

	EnsureHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame_data[0]->command_allocators[0].Get(), nullptr, IID_PPV_ARGS(command_list.Replace())));
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
			static const D3D12_DESCRIPTOR_RANGE table = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, GPU::MaxBoundShaderResources, 0, 0, 0 }; // Make sure pointer is valid outside this block
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

		frame_fence.WaitForFence(frame->fence_value, frame_fence_event);
	}

	// TODO: Empty all pools
}

void GPU_DX12::CreateSwapChain(void* hwnd, u32 width, u32 height) {
	swap_chain_dimensions = { width, height };
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
	OnSwapChainUpdated();
}

void GPU_DX12::ResizeSwapChain(void*, u32 width, u32 height) {
	// TODO: Resizing the swap chain resets which buffer we need to use
	//	wait for idle here so we can reset frame index? 
	//	or set up render targets for each frame after we resize
	// Just getting the frame index seems to work for now, but seems like a race condition waiting to happen.

	for (u32 n = 0; n < frame_count; ++n) {
		frame_data[n]->render_target.Clear();
	}
	swap_chain_dimensions = { width, height };
	EnsureHR(swap_chain->ResizeBuffers(frame_count, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));

	OnSwapChainUpdated();
}

void GPU_DX12::OnSwapChainUpdated() {
	u32 width = swap_chain_dimensions[0], height = swap_chain_dimensions[1];
	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
	for (u32 n = 0; n < frame_count; ++n) {
		EnsureHR(swap_chain->GetBuffer(n, IID_PPV_ARGS(frame_data[n]->render_target.Replace())));
		device->CreateRenderTargetView(frame_data[n]->render_target.Get(), nullptr, rtv_handle);
		frame_data[n]->render_target_view = rtv_handle;
		rtv_handle.ptr += rtv_descriptor_size;
	}
	frame_index = swap_chain->GetCurrentBackBufferIndex();

	viewport = D3D12_VIEWPORT{ 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	scissor_rect = D3D12_RECT{ 0, 0, (LONG)width, (LONG)height };
}

GPUFrameInterface* GPU_DX12::BeginFrame() {
	auto& frame = frame_data[frame_index];

	frame_fence.WaitForFence(frame->fence_value, frame_fence_event);
	frame->used_command_allocators = 0;
	
	ID3D12CommandAllocator* command_allocator = frame->GetCommandAllocator();
	EnsureHR(command_list->Reset(command_allocator, nullptr));

	command_list->RSSetViewports(1, &viewport);
	command_list->RSSetScissorRects(1, &scissor_rect);

	auto present_to_rt = ResourceBarrier{ frame->render_target.Get(), ResourceState::Present, ResourceState::RenderTarget };
	command_list->ResourceBarrier(1, &present_to_rt);

	command_list->OMSetRenderTargets(1, &frame->render_target_view, false, nullptr);
	float clear_color[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	command_list->ClearRenderTargetView(frame->render_target_view, clear_color, 0, nullptr);

	EnsureHR(command_list->Close());

	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(1, command_lists);
	return frame;
}

void GPU_DX12::EndFrame(GPUFrameInterface* frame_interface) {
	auto frame = frame_data[frame_index];
	CHECK(frame == frame_interface);

	ID3D12CommandAllocator* command_allocator = frame->GetCommandAllocator();
	EnsureHR(command_list->Reset(command_allocator, nullptr));

	auto rt_to_present = ResourceBarrier{ frame->render_target.Get(), ResourceState::RenderTarget, ResourceState::Present };
	command_list->ResourceBarrier(1, &rt_to_present);

	EnsureHR(command_list->Close());

	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(1, command_lists);

	EnsureHR(swap_chain->Present(1, 0));

	frame->linear_allocator.FrameReset();
	frame->descriptor_table_allocator.FrameReset();

	// Update fence value
	frame->fence_value = frame_fence.SubmitFence(command_queue.Get());
	frame_index = swap_chain->GetCurrentBackBufferIndex();
}





VertexShaderID GPU_DX12::CompileVertexShaderHLSL(const char* entry_point, PointerRange<const u8> code) {
	VertexShaderID id = registered_vertex_shaders.AddDefaulted();
	COMPtr<ID3DBlob>& compiled_shader = registered_vertex_shaders[id].CompiledShader;
	DX::CompileShaderHLSL(compiled_shader.Replace(), "vs_5_0", entry_point, code);
	CHECK(compiled_shader.Get());

	VertexShaderInputs& inputs = registered_vertex_shaders[id].Inputs;

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
	for (tuple<u32, DX::VertexShaderInputElement&> it : Zip(Iota<u32>(), inputs.InputElements)) {
		D3D12_SIGNATURE_PARAMETER_DESC input_param;
		reflector->GetInputParameterDesc(get<0>(it), &input_param);

		get<1>(it) = ParseInputParameter(input_param);
	}

	return id;
}
PixelShaderID GPU_DX12::CompilePixelShaderHLSL(const char* entry_point, PointerRange<const u8> code) {
	PixelShaderID id = registered_pixel_shaders.AddDefaulted();
	ID3DBlob** compiled_shader = registered_pixel_shaders[id].Replace();
	DX::CompileShaderHLSL(compiled_shader, "ps_5_0", entry_point, code);
	CHECK(*compiled_shader);
	return id;
}

ProgramID GPU_DX12::LinkProgram(VertexShaderID vertex_shader, PixelShaderID pixel_shader) {
	ProgramID id = linked_programs.AddDefaulted();
	linked_programs[id] = { vertex_shader, pixel_shader };
	return id;
}

GPU::PipelineStateID GPU_DX12::CreatePipelineState(const GPU::PipelineStateDesc& desc) {
	auto found = Find(pipeline_states, [&](tuple<PipelineStateID, PipelineState&> ps) {
		if (get<1>(ps).Desc == desc) {
			return true;
		}
		return false;
	});
	if (!found.IsEmpty()) {
		return get<0>(found.Front());
	}


	PipelineStateID id = pipeline_states.AddDefaulted();
	PipelineState& ps = pipeline_states[id];
	ps.Desc = desc;
	LinkedProgram program = linked_programs[desc.Program];
	Array<D3D12_INPUT_ELEMENT_DESC> input_layout; // TODO: fixed array

	for (u32 slot_idx = 0; slot_idx < desc.StreamFormat.Slots.Num(); ++slot_idx) {
		u32& stride = ps.VBStrides.AddZeroed(1).Front();
		const GPU::StreamSlotDesc& slot = desc.StreamFormat.Slots[slot_idx];
		for (GPU::StreamElementDesc& elem : slot.Elements) {
			stride += GPU::GetStreamElementSize(elem);
			input_layout.Add(D3D12_INPUT_ELEMENT_DESC{
				DX::GetSemanticName(elem.Semantic),
				elem.SemanticIndex,
				DX::GetStreamElementFormat(elem),
				slot_idx,
				D3D12_APPEND_ALIGNED_ELEMENT,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0
			});
		}
	}

	auto pipeline_desc = GraphicsPipelineStateDesc{ root_signature.Get() }
		.VS(registered_vertex_shaders[program.VertexShader].CompiledShader)
		.PS(registered_pixel_shaders[program.PixelShader])
		.DepthEnable(false)
		.BlendState(desc.BlendState)
		.PrimType(PrimType::Triangle)
		.RenderTargets(DXGI_FORMAT_R8G8B8A8_UNORM)
		.InputLayout(input_layout.Data(), (u32)input_layout.Num());
	EnsureHR(device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(ps.PSO.Replace())));

	return id;
}
void GPU_DX12::DestroyPipelineState(GPU::PipelineStateID) {}

ConstantBufferID GPU_DX12::CreateConstantBuffer(PointerRange<const u8> data) {
	COMPtr<ID3D12Resource> cbuffer;
	auto cbuffer_desc = BufferDesc{ data.Size() };
	EnsureHR(device->CreateCommittedResource(
		&upload_heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&cbuffer_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(cbuffer.Replace())
	));
	void* mapped = nullptr;
	D3D12_RANGE read_range{ 0, 0 };
	EnsureHR(cbuffer->Map(0, &read_range, &mapped));
	memcpy(mapped, &data.Front(), data.Size());
	cbuffer->Unmap(0, nullptr);

	ConstantBufferID id = constant_buffers.Emplace(ConstantBuffer{ cbuffer, { cbuffer->GetGPUVirtualAddress(), (u32)data.Size() } });

	return id;
}

void GPU_DX12::DestroyConstantBuffer(ConstantBufferID) {
	CHECKF(false, "Not yet implemented");
}

VertexBufferID GPU_DX12::CreateVertexBuffer(PointerRange<const u8> data) {
	VertexBufferID id = vertex_buffers.AddDefaulted();
	CHECK(vertex_buffers[id].Resource.Get() == nullptr);
	vertex_buffers[id].Size = (u32)data.Size();

	auto vbuffer_desc = BufferDesc{ (u32)data.Size() };
	EnsureHR(device->CreateCommittedResource(
		&upload_heap_properties, // TODO: Upload & copy properly
		D3D12_HEAP_FLAG_NONE,
		&vbuffer_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(vertex_buffers[id].Resource.Replace()))
	);

	D3D12_RANGE read_range{ 0, 0 };
	void* mapped = nullptr;
	EnsureHR(vertex_buffers[id].Resource->Map(0, &read_range, &mapped));
	memcpy(mapped, &data.Front(), data.Size());
	vertex_buffers[id].Resource->Unmap(0, nullptr);

	return id;
}

void GPU_DX12::DestroyVertexBuffer(GPU::VertexBufferID) {
	CHECKF(false, "Not yet implemented");
}

IndexBufferID GPU_DX12::CreateIndexBuffer(mu::PointerRange<const u8> data) {
	IndexBufferID id = index_buffers.AddDefaulted();
	index_buffers[id].Size = (u32)data.Size();

	auto ibuffer_desc = BufferDesc{ (u32)data.Size() };
	EnsureHR(device->CreateCommittedResource(
		&upload_heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&ibuffer_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(index_buffers[id].Resource.Replace()))
		);

	D3D12_RANGE read_range{ 0, 0 };
	void* mapped = nullptr;
	EnsureHR(index_buffers[id].Resource->Map(0, &read_range, &mapped));
	memcpy(mapped, &data.Front(), data.Size());
	index_buffers[id].Resource->Unmap(0, nullptr);

	return id;
}

void GPU_DX12::DestroyIndexBuffer(GPU::IndexBufferID) {
	CHECKF(false, "Not yet implemented");
}

TextureID GPU_DX12::CreateTexture2D(u32 width, u32 height, GPU::TextureFormat format, PointerRange<const u8> data) {
	TextureID id = textures.AddDefaulted();
	Texture& texture = textures[id];

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
		D3D12_SUBRESOURCE_DATA source{ &data.Front(), DX::CalcRowPitch(format, width), DX::CalcRowPitch(format, width) * 1 };
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

ShaderResourceListID GPU_DX12::CreateShaderResourceList(const GPU::ShaderResourceListDesc& desc) {
	ShaderResourceListID id = shader_resource_lists.AddDefaulted();
	ShaderResourceList& srl = shader_resource_lists[id];
	srl.Desc = desc;

	return id;
}


D3D12_CPU_DESCRIPTOR_HANDLE GPU_DX12::GetRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE backbuffer, RenderTargetID id) {
	CHECK(id == RenderTargetID{});
	return backbuffer; // TODO
}

D3D12_CONSTANT_BUFFER_VIEW_DESC GPU_DX12::GetConstantBufferViewDesc(GPU_DX12_Frame* frame, ConstantBufferID id) {
	if ((size_t)id < max_persistent_cbs) {
		return constant_buffers[id].ViewDesc;
	}
	else {
		return frame->temp_cbs[(size_t)id - max_persistent_cbs];
	}
}

D3D12_VERTEX_BUFFER_VIEW GPU_DX12::GetVertexBufferView(GPU_DX12_Frame* frame, VertexBufferID vb_id, u32 stride) {
	if (vb_id == VertexBufferID{}) {
		return {};
	}
	else if ((size_t)vb_id >= max_persistent_vbs) {
		auto tmp = frame->temp_vbs[(size_t)vb_id - max_persistent_vbs];
		return { tmp.BufferLocation, tmp.SizeInBytes, stride };
	}
	else {
		return vertex_buffers[vb_id].MakeVertexBufferView(stride);
	}
}


D3D12_INDEX_BUFFER_VIEW GPU_DX12::GetIndexBufferView(GPU_DX12_Frame* frame, IndexBufferID id) {
	if (id == IndexBufferID{}) {
		return { };
	}
	else if ((size_t)id >= max_persistent_ibs) {
		auto tmp = frame->temp_ibs[(size_t)id - max_persistent_ibs];
		return { tmp.BufferLocation, tmp.SizeInBytes, DXGI_FORMAT_R16_UINT };
	}
	else {
		return index_buffers[id].MakeIndexBufferView(DXGI_FORMAT_R16_UINT);
	}
}

void GPU_DX12::SubmitPass(const GPU::RenderPass& pass) {
	auto frame = frame_data[frame_index];

	ID3D12CommandAllocator* command_allocator = frame->GetCommandAllocator();
	EnsureHR(command_list->Reset(command_allocator, nullptr));

	command_list->SetGraphicsRootSignature(root_signature.Get());

	// TODO: Need to handle reallocating new heaps and putting them in the command list. What's the cost of SetDescriptorHeaps?
	ID3D12DescriptorHeap* heaps[] = { frame->descriptor_table_allocator.m_heap.Get() };
	command_list->SetDescriptorHeaps((u32)ArraySize(heaps), heaps);

	// Bind RTs for pass
	CHECK(pass.RenderTargets.Num() <= GPU::MaxRenderTargets);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvs[GPU::MaxRenderTargets];
	for (tuple<D3D12_CPU_DESCRIPTOR_HANDLE&, RenderTargetID> it : Zip(rtvs, pass.RenderTargets)) {
		get<0>(it) = GetRenderTargetView(frame->render_target_view, get<1>(it));
	}
	command_list->OMSetRenderTargets((u32)pass.RenderTargets.Num(), rtvs, false, nullptr);

	// TODO: Make part of render pass
	command_list->RSSetViewports(1, &viewport);
	command_list->RSSetScissorRects(1, &scissor_rect);

	// Translate draw commands into CommandList calls
	for (const GPU::DrawItem& item : pass.DrawItems) {
		const PipelineState& pipeline_state = pipeline_states[item.PipelineState];
		ID3D12PipelineState* pso = pipeline_state.PSO;
		CHECK(pso);
		command_list->SetPipelineState(pso);
		command_list->IASetPrimitiveTopology(CommonToDX12(item.Command.PrimTopology));

		//FixedArray<D3D12_CONSTANT_BUFFER_VIEW_DESC, GPU::MaxBoundConstantBuffers> cbs;
		//for (ConstantBufferID id : item.BoundResources.ConstantBuffers) {
		//	cbs.Add(GetConstantBufferViewDesc(id));
		//}
		CHECK(item.BoundResources.ConstantBuffers.Num() <= 1); // TODO: Update root signature
		if (item.BoundResources.ConstantBuffers.Num() > 0 && item.BoundResources.ConstantBuffers[0] != ConstantBufferID{}) {
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = GetConstantBufferViewDesc(frame, item.BoundResources.ConstantBuffers[0]);
			command_list->SetGraphicsRootConstantBufferView(0, cbv_desc.BufferLocation);
		}

		FixedArray<TextureID, GPU::MaxBoundShaderResources> bound_resources;
		bound_resources.AddMany(GPU::MaxBoundShaderResources, TextureID{ u32_max });
		for (ShaderResourceListID id : item.BoundResources.ResourceLists) {
			const ShaderResourceList& list = shader_resource_lists[id];
			for (u32 i = list.Desc.StartSlot; i < list.Desc.Textures.Num() && (i + list.Desc.StartSlot < GPU::MaxBoundShaderResources); ++i) {
				bound_resources[i] = list.Desc.Textures[i - list.Desc.StartSlot];
			}
		}

		DX12DescriptorTable srv_table = frame->descriptor_table_allocator.AllocateTable((u32)bound_resources.Num());
		for (u32 i = 0; i < bound_resources.Num(); ++i) {
			TextureID id = bound_resources[i];
			if (id == TextureID{}) { continue; }
			auto& texture = textures[id];
			device->CreateShaderResourceView(texture.Resource.Get(), &texture.SRVDesc, srv_table.GetCPUHandle(i)); // TODO: Copy descriptors instead of recreating them
		}

		command_list->SetGraphicsRootDescriptorTable(1, srv_table.GetGPUHandle(0));

		const GPU::StreamSetup& stream_setup = item.StreamSetup;
		FixedArray<D3D12_VERTEX_BUFFER_VIEW, GPU::MaxStreamSlots> vbs;

		for (tuple<VertexBufferID, u32> it : Zip(item.StreamSetup.VertexBuffers, pipeline_state.VBStrides)) {
			VertexBufferID vb_id = get<0>(it);
			u32 stride = get<1>(it);
			vbs.Add(GetVertexBufferView(frame, vb_id, stride));
		}

		command_list->IASetVertexBuffers(0, (u32)vbs.Num(), vbs.Data());
		if (stream_setup.IndexBuffer != IndexBufferID{}) {
			D3D12_INDEX_BUFFER_VIEW ib = GetIndexBufferView(frame, stream_setup.IndexBuffer);

			//ib.BufferLocation += item.Command.IndexOffset * sizeof(u16); // TODO: variable index size
			command_list->IASetIndexBuffer(&ib);
			command_list->DrawIndexedInstanced(item.Command.VertexOrIndexCount, 1, item.Command.IndexOffset, 0, 0);
		}
		else {
			command_list->IASetIndexBuffer(nullptr);
			command_list->DrawInstanced(item.Command.VertexOrIndexCount, 1, 0, 0);
		}
	}

	EnsureHR(command_list->Close());

	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(1, command_lists);
}


DX12LinearAllocator::DX12LinearAllocator(GPU_DX12* gpu) : m_gpu(gpu) {
	m_max = 2 * 1024 * 1024;
	auto desc = BufferDesc{ m_max };
	EnsureHR(gpu->device->CreateCommittedResource(
		&gpu->upload_heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_data.Replace())
	));
	EnsureHR(m_data->Map(0, nullptr, &(void*)m_data_cpu_ptr));
}

GPUAllocation DX12LinearAllocator::Allocate(u32 size) {
	size = AlignPow2(size, 256);
	CHECKF(m_used + size <= m_max, "Ran out of space trying to allocate ", size, " bytes");

	D3D12_GPU_VIRTUAL_ADDRESS addr = m_data->GetGPUVirtualAddress() + m_used;
	void* cpu_addr = m_data_cpu_ptr + m_used;
	m_used += size;

	return { addr, size, cpu_addr };
}

ConstantBufferID GPU_DX12_Frame::GetTemporaryConstantBuffer(PointerRange<const u8> data) {
	auto addr = linear_allocator.Allocate((u32)data.Size());
	memcpy(addr.CPUAddress, &data.Front(), data.Size());
	auto index = temp_cbs.Add({ addr.GPUAddress, (u32)data.Size() });
	return ConstantBufferID{ index + m_gpu->max_persistent_cbs };
}

VertexBufferID GPU_DX12_Frame::GetTemporaryVertexBuffer(mu::PointerRange<const u8> data) {
	auto addr = linear_allocator.Allocate((u32)data.Size());
	memcpy(addr.CPUAddress, &data.Front(), data.Size());
	auto index = temp_vbs.Add({ addr.GPUAddress, (u32)data.Size(), 0 });
	return VertexBufferID{ index + m_gpu->max_persistent_vbs };
}
IndexBufferID GPU_DX12_Frame::GetTemporaryIndexBuffer(mu::PointerRange<const u8> data) {
	auto addr = linear_allocator.Allocate((u32)data.Size());
	memcpy(addr.CPUAddress, &data.Front(), data.Size());
	auto index = temp_ibs.Add({ addr.GPUAddress, (u32)data.Size(), DXGI_FORMAT_UNKNOWN });
	return IndexBufferID{ index + m_gpu->max_persistent_ibs };
}

ID3D12CommandAllocator* GPU_DX12_Frame::GetCommandAllocator() {
	if (used_command_allocators >= command_allocators.Num()) {
		size_t index = command_allocators.Add({});

		D3D12_COMMAND_QUEUE_DESC queue_desc = {};
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		EnsureHR(m_gpu->device->CreateCommandAllocator(queue_desc.Type, IID_PPV_ARGS(command_allocators[index].Replace())));
	}
	ID3D12CommandAllocator* alloc = command_allocators[used_command_allocators];
	alloc->Reset();
	++used_command_allocators;
	return alloc;
}

DX12DescriptorTableLinearAllocator::DX12DescriptorTableLinearAllocator(GPU_DX12* gpu) 
	: m_gpu(gpu) {
	m_descriptor_size = m_gpu->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_total_descriptors = 128;
	m_used_descriptors = 0;
	
	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.NodeMask = 0;
	desc.NumDescriptors = m_total_descriptors;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	EnsureHR(m_gpu->device->CreateDescriptorHeap(
		&desc,
		IID_PPV_ARGS(m_heap.Replace()))
	);
}

DX12DescriptorTable DX12DescriptorTableLinearAllocator::AllocateTable(u32 num_descriptors) {
	CHECK(m_used_descriptors + num_descriptors <= m_total_descriptors);
	DX12DescriptorTable table{ 
		m_heap->GetCPUDescriptorHandleForHeapStart(), 
		m_heap->GetGPUDescriptorHandleForHeapStart(), 
		m_descriptor_size, 
		num_descriptors 
	};
	table.m_cpu_handle.ptr += m_used_descriptors * m_descriptor_size;
	table.m_gpu_handle.ptr += m_used_descriptors * m_descriptor_size;

	m_used_descriptors += num_descriptors;
	return table;
}

