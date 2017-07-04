#pragma once

#include "GPU_Common.h"
#include "mu-core/PointerRange.h"

namespace GPU_DX12 {
	using GPUCommon::StreamFormatID;
	using GPUCommon::VertexBufferID;
	using GPUCommon::IndexBufferID;
	using GPUCommon::InputAssemblerConfigID;
	using GPUCommon::VertexShaderID;
	using GPUCommon::PixelShaderID;
	using GPUCommon::ProgramID;
	using GPUCommon::ConstantBufferID;
	using GPUCommon::TextureID;
	using GPUCommon::ShaderResourceListID;

	void Init();
	void Shutdown();
	void RecreateSwapChain(void* hwnd, u32 width, u32 height);

	void BeginFrame();
	void EndFrame();

	void SubmitPass(const GPUCommon::RenderPass& pass);

	StreamFormatID RegisterStreamFormat(const GPUCommon::StreamFormatDesc& format);
	InputAssemblerConfigID RegisterInputAssemblyConfig(GPUCommon::StreamFormatID format, mu::PointerRange<const VertexBufferID> vertex_buffers, IndexBufferID index_buffer);

	VertexShaderID CompileVertexShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code);
	PixelShaderID CompilePixelShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code);
	ProgramID LinkProgram(GPUCommon::VertexShaderID vertex_shader, GPUCommon::PixelShaderID pixel_shader);

	ConstantBufferID CreateConstantBuffer(mu::PointerRange<const u8> data);
	VertexBufferID CreateVertexBuffer(mu::PointerRange<const u8> data);
	IndexBufferID CreateIndexBuffer();

	TextureID CreateTexture2D(u32 width, u32 height, GPUCommon::TextureFormat format, mu::PointerRange<const u8> data);

	ShaderResourceListID CreateShaderResourceList(const GPUCommon::ShaderResourceListDesc& desc);
};

namespace GPU = GPU_DX12;