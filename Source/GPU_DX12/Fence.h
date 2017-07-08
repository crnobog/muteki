#pragma once

#include "DX12Utils.h"

namespace DX12Util {
	struct Fence {
		DX::COMPtr<ID3D12Fence> fence;
		u64						last_value = 0;

		u64 SubmitFence(ID3D12CommandQueue* command_queue) {
			++last_value;
			EnsureHR(command_queue->Signal(fence.Get(), last_value));
			return last_value;
		}
		bool IsFenceComplete(u64 value) {
			return fence->GetCompletedValue() >= value;
		}
		void WaitForFence(u64 value, HANDLE event) {
			if (!IsFenceComplete(value)) {
				EnsureHR(fence->SetEventOnCompletion(value, event));
				WaitForSingleObjectEx(event, INFINITE, false);
			}
		}
	};
}