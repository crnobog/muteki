#include "mu-core/Debug.h"
#include "GPU_DX12.h"

#include "mu-core/RefCountPtr.h"
#include "mu-core/Vectors.h"

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define EnsureHR(expr) \
	ENSURE(SUCCEEDED((expr)))

namespace {
	template<typename T, size_t N> constexpr size_t ArraySize(const T(&)[N]) { return N; }

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

	enum class ResourceState : int32_t {
		Present = D3D12_RESOURCE_STATE_PRESENT,
		RenderTarget = D3D12_RESOURCE_STATE_RENDER_TARGET,
	};

	struct ResourceBarrier : D3D12_RESOURCE_BARRIER {
		ResourceBarrier(ID3D12Resource* resource, ResourceState before, ResourceState after) {
			Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			Transition.pResource = resource;
			Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			Transition.StateBefore = (D3D12_RESOURCE_STATES)before;
			Transition.StateAfter = (D3D12_RESOURCE_STATES)after;
		}
	};

	struct BufferDesc : D3D12_RESOURCE_DESC {
		BufferDesc(uint32_t width) {
			Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			Alignment = 0;
			Width = width;
			Height = 1;
			DepthOrArraySize = 1;
			MipLevels = 1;
			Format = DXGI_FORMAT_UNKNOWN;
			SampleDesc = { 1, 0 };
			Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			Flags = D3D12_RESOURCE_FLAG_NONE;
		}
	};

	enum class PrimType { 
		Triangle = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, 
	};

	static_assert(D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT == 8, "D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT != 8");
	const D3D12_BLEND_DESC DefaultBlendDesc = { false, false, {
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

	struct GraphicsPipelineStateDesc : D3D12_GRAPHICS_PIPELINE_STATE_DESC {
		typedef D3D12_GRAPHICS_PIPELINE_STATE_DESC Base;
		GraphicsPipelineStateDesc(ID3D12RootSignature* root_signature) {
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

		GraphicsPipelineStateDesc& VS(ID3DBlob* blob) {
			Base::VS = { blob->GetBufferPointer(), blob->GetBufferSize() };
			return *this;
		}
		GraphicsPipelineStateDesc& PS(ID3DBlob* blob) {
			Base::PS = { blob->GetBufferPointer(), blob->GetBufferSize() };
			return *this;
		}
		GraphicsPipelineStateDesc& DepthEnable(bool enable) {
			Base::DepthStencilState.DepthEnable = enable;
			return *this;
		}
		GraphicsPipelineStateDesc& PrimType(PrimType type) {
			Base::PrimitiveTopologyType = (D3D12_PRIMITIVE_TOPOLOGY_TYPE)type;
			return *this;
		}
		GraphicsPipelineStateDesc& RenderTargets(DXGI_FORMAT format) {
			Base::NumRenderTargets = 1;
			Base::RTVFormats[0] = format;
			return *this;
		}
		GraphicsPipelineStateDesc& InputLayout(D3D12_INPUT_ELEMENT_DESC* elements, uint32_t num_elements) {
			Base::InputLayout.NumElements = num_elements;
			Base::InputLayout.pInputElementDescs = elements;
			return *this;
		}
	};

	struct Fence {
		COMPtr<ID3D12Fence>				fence;
		uint64_t						last_value = 0;

		uint64_t SubmitFence(ID3D12CommandQueue* command_queue) {
			++last_value;
			EnsureHR(command_queue->Signal(fence.Get(), last_value));
			return last_value;
		}
		bool IsFenceComplete(uint64_t value) {
			return fence->GetCompletedValue() >= value;
		}
		void WaitForFence(uint64_t value, HANDLE event) {
			if (!IsFenceComplete(value)) {
				EnsureHR(fence->SetEventOnCompletion(value, event));
				WaitForSingleObjectEx(event, INFINITE, false);
			}
		}
	};

	const uint32_t frame_count = 2;
	const D3D12_HEAP_PROPERTIES upload_heap_properties{ D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };

	COMPtr<IDXGIFactory4>				dxgi_factory;
	COMPtr<ID3D12Device>				device;
	COMPtr<ID3D12CommandQueue>			command_queue;
	COMPtr<ID3D12GraphicsCommandList>	command_list;
	COMPtr<ID3D12DescriptorHeap>		rtv_heap;
	COMPtr<ID3D12RootSignature>			root_signature;

	COMPtr<IDXGISwapChain3>				swap_chain;
	D3D12_VIEWPORT						viewport = {};
	D3D12_RECT							scissor_rect = {};

	COMPtr<ID3D12Resource>				vertex_buffer;
	D3D12_VERTEX_BUFFER_VIEW			vertex_buffer_view = {};

	COMPtr<ID3D12PipelineState>			pipeline_state;

	UINT rtv_descriptor_size = 0;

	uint32_t frame_index = 0;

	struct {
		COMPtr<ID3D12Resource>			render_target;
		D3D12_CPU_DESCRIPTOR_HANDLE		render_target_view;
		COMPtr<ID3D12CommandAllocator>	command_allocator;
		uint64_t						fence_value;
	} frame_data[frame_count];

	Fence	frame_fence;
	HANDLE	frame_fence_event;
}

namespace {
	void WaitForFence(ID3D12Fence* fence, HANDLE event, UINT64 value) {
		if (ENSURE(fence != nullptr)
			&& ENSURE(event != nullptr)
			&& fence->GetCompletedValue() < value) {
			EnsureHR(fence->SetEventOnCompletion(value, event));
			WaitForSingleObjectEx(event, INFINITE, false);
		}
	}
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

	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	EnsureHR(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(command_queue.Replace())));

	for (uint32_t i = 0; i < frame_count; ++i) {
		EnsureHR(device->CreateCommandAllocator(queue_desc.Type, IID_PPV_ARGS(frame_data[i].command_allocator.Replace())));
	}

	EnsureHR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(frame_fence.fence.Replace())));

	EnsureHR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame_data[0].command_allocator.Get(), nullptr, IID_PPV_ARGS(command_list.Replace())));
	command_list->Close(); // Force closed so it doesn't hold on to command allocator 0

	D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
	rtv_heap_desc.NumDescriptors = frame_count;
	rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	EnsureHR(device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(rtv_heap.Replace())));

	rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(rtv_heap_desc.Type);

	frame_fence_event = CreateEvent(nullptr, false, false, L"Frame Fence Event");

	// Empty root signature
	{
		D3D12_ROOT_SIGNATURE_DESC root_signature_desc = { 0, nullptr, 0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
		COMPtr<ID3DBlob> signature_blob, error_blob;
		EnsureHR(D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, signature_blob.Replace(), error_blob.Replace()));
		EnsureHR(device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(), IID_PPV_ARGS(root_signature.Replace())));
	}

	// Vertex buffer
	{
		struct Vertex {
			Vec3 position;
		};
		Vertex triangle_vertices[] = {
			{ 0.0f, 0.5f, 0.0f }, { 0.5f, -0.5f, 0.0f }, { -0.5f, -0.5f, 0.0f }
		};
		const uint32_t vbuffer_size = sizeof(triangle_vertices);
		auto vbuffer_desc = BufferDesc{ vbuffer_size };
		EnsureHR(device->CreateCommittedResource(
			&upload_heap_properties,
			D3D12_HEAP_FLAG_NONE,
			&vbuffer_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(vertex_buffer.Replace()))
		);


		D3D12_RANGE read_range{ 0, 0 };
		Vertex* mapped = nullptr;
		EnsureHR(vertex_buffer->Map(0, &read_range, &(void*)mapped));
		memcpy(mapped, &triangle_vertices, vbuffer_size);
		vertex_buffer->Unmap(0, nullptr);

		vertex_buffer_view = { vertex_buffer->GetGPUVirtualAddress(), vbuffer_size, sizeof(Vertex) };
	}

	// Shaders and pipeline state
	{
		COMPtr<ID3DBlob> vertex_shader, pixel_shader, vshader_errors, pshader_errors;

		const char shader_code[] =
			"float4 vs_main(float4 in_pos : POSITION) : SV_POSITION\n"
			"{\n"
			"    return in_pos;\n"
			"}\n"
			"\n"
			"float4 ps_main(float4 in_pos : SV_POSITION) : SV_TARGET\n"
			"{\n"
			"    return float4(1.0f, 0.0f, 0.0f, 1.0f);\n"
			"}\n"
			"\n";
		size_t shader_code_size = sizeof(shader_code);

		EnsureHR(D3DCompile(
			shader_code, 
			shader_code_size, 
			nullptr, 
			nullptr, 
			nullptr, 
			"vs_main", 
			"vs_5_0", 
			0, 
			0, 
			vertex_shader.Replace(), 
			vshader_errors.Replace()));
		
		EnsureHR(D3DCompile(
			shader_code, 
			shader_code_size, 
			nullptr, 
			nullptr, 
			nullptr, 
			"ps_main", 
			"ps_5_0", 
			0, 
			0, 
			pixel_shader.Replace(), 
			pshader_errors.Replace()));

		// TODO: Make builder?
		D3D12_INPUT_ELEMENT_DESC input_layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		auto pipeline_desc = GraphicsPipelineStateDesc{ root_signature.Get() }
			.VS(vertex_shader)
			.PS(pixel_shader)
			.DepthEnable(false)
			.PrimType(PrimType::Triangle)
			.RenderTargets(DXGI_FORMAT_R8G8B8A8_UNORM)
			.InputLayout(input_layout, (uint32_t)ArraySize(input_layout));
		EnsureHR(device->CreateGraphicsPipelineState(&pipeline_desc, IID_PPV_ARGS(pipeline_state.Replace())));
	}
}

void GPU_DX12::Shutdown() {
	for (int32_t i = 0; i < frame_count; ++i) {
		auto& frame = frame_data[i];

		frame_fence.WaitForFence(frame.fence_value, frame_fence_event);
	}
}

void GPU_DX12::RecreateSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
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
			hwnd,
			&swap_chain_desc,
			nullptr, // fullscreen desc
			nullptr, // restrict output
			swap_chain_tmp.Replace()));
		EnsureHR(dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

		EnsureHR(swap_chain_tmp->QueryInterface(swap_chain.Replace()));
	}
	frame_index = swap_chain->GetCurrentBackBufferIndex();

	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
	for (uint32_t n = 0; n < frame_count; ++n) {
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

	EnsureHR(command_list->Reset(frame.command_allocator.Get(), pipeline_state.Get()));

	command_list->SetGraphicsRootSignature(root_signature.Get());
	command_list->RSSetViewports(1, &viewport);
	command_list->RSSetScissorRects(1, &scissor_rect);

	auto present_to_rt = ResourceBarrier{ frame.render_target.Get(), ResourceState::Present, ResourceState::RenderTarget };
	command_list->ResourceBarrier(1, &present_to_rt);

	command_list->OMSetRenderTargets(1, &frame.render_target_view, false, nullptr);
	float clear_color[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	command_list->ClearRenderTargetView(frame.render_target_view, clear_color, 0, nullptr);

	command_list->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
	command_list->DrawInstanced(3, 1, 0, 0);

	auto rt_to_present = ResourceBarrier{ frame.render_target.Get(), ResourceState::RenderTarget, ResourceState::Present };
	command_list->ResourceBarrier(1, &rt_to_present);

	EnsureHR(command_list->Close());

	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(1, command_lists);
}

void GPU_DX12::EndFrame() {
	auto& frame = frame_data[frame_index];

	EnsureHR(swap_chain->Present(1, 0));

	// Update fence value
	frame.fence_value = frame_fence.SubmitFence(command_queue.Get());
	frame_index = swap_chain->GetCurrentBackBufferIndex();
}
