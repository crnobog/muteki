#pragma once

#include "GPUInterface.h"
#include "ShaderManager.h"

#include <memory>

struct ImguiImplInterface {
	virtual void Init(void* window_handle, GPUInterface* gpu, ShaderManager* shader_manager) = 0;
	virtual void Shutdown() = 0;
	virtual void BeginFrame(Vector<double, 2> mouse_pos, Vector<bool, 3> mouse_buttons, Vec2 scroll) = 0;
	virtual void Render(GPUFrameInterface* gpu_frame) = 0;
};

std::unique_ptr<ImguiImplInterface> CreateImguiImpl();