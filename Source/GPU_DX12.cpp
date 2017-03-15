#include "mu-core/Debug.h"
#include "GPU_DX12.h"

#include "mu-core/RefCountPtr.h"

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

	COMPtr<IDXGIFactory4>				dxgi_factory;
	COMPtr<ID3D12Device>				device;
	COMPtr<ID3D12CommandQueue>			command_queue;
	COMPtr<ID3D12GraphicsCommandList>	command_list;
	COMPtr<ID3D12DescriptorHeap>		rtv_heap;
	COMPtr<IDXGISwapChain3>				swap_chain;

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

	for (uint32_t i = 0; i < frame_count; ++i)
	{
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
}

void GPU_DX12::RecreateSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
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

void GPU_DX12::BeginFrame()
{
	auto& frame = frame_data[frame_index];

	frame_fence.WaitForFence(frame.fence_value, frame_fence_event);

	EnsureHR(frame.command_allocator->Reset());

	EnsureHR(command_list->Reset(frame.command_allocator.Get(), nullptr));

	auto present_to_rt = ResourceBarrier{frame.render_target.Get(), ResourceState::Present, ResourceState::RenderTarget };
	command_list->ResourceBarrier(1, &present_to_rt);

	command_list->OMSetRenderTargets(1, &frame.render_target_view, false, nullptr);
	float clear_color[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	command_list->ClearRenderTargetView(frame.render_target_view, clear_color, 0, nullptr);

	auto rt_to_present = ResourceBarrier{ frame.render_target.Get(), ResourceState::RenderTarget, ResourceState::Present };
	command_list->ResourceBarrier(1, &rt_to_present);

	EnsureHR(command_list->Close());

	ID3D12CommandList* command_lists[] = { command_list.Get() };
	command_queue->ExecuteCommandLists(1, command_lists);
}

void GPU_DX12::EndFrame()
{
	auto& frame = frame_data[frame_index];

	EnsureHR(swap_chain->Present(1, 0));

	// Update fence value
	frame.fence_value = frame_fence.SubmitFence(command_queue.Get());
	frame_index = swap_chain->GetCurrentBackBufferIndex();
}
