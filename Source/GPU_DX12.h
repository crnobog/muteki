#pragma once

#include "GPU_Common.h"

namespace GPU_DX12 {

	void Init();
	void Shutdown();
	void RecreateSwapChain(void* hwnd, u32 width, u32 height);

	void BeginFrame();
	void RenderTestFrame();
	void EndFrame();

	void SubmitPass(const RenderPass& pass);

	StreamFormatID RegisterStreamFormat(const StreamFormat& format);

	ShaderID CompileShader();
	ProgramID LinkProgram();

	ConstantBufferID CreateConstantBuffer();
	VertexBufferID CreateVertexBuffer();
	IndexBufferID CreateIndexBuffer();
};

namespace GPU = GPU_DX12;