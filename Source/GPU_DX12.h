#pragma once

#include <d3d12.h>
#include <cstdint>

struct GPU_DX12 {
	static void Init();
	static void Shutdown();
	static void RecreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);

	static void BeginFrame();
	static void EndFrame();
};

typedef GPU_DX12 GPU;