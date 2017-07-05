#pragma once

#include "mu-core/PrimitiveTypes.h"
#include "mu-core/PointerRange.h"
#include "mu-core/FixedArray.h"

#pragma warning(push) 
#pragma warning(disable : 4100)

#define DECLARE_HANDLE(NAME, INTERNAL) \
struct NAME { \
	INTERNAL Index = INTERNAL##_max; \
	explicit operator size_t() { return Index; } \
	constexpr NAME() = default; \
	constexpr explicit NAME(size_t i) : Index((INTERNAL)i) {} \
	constexpr explicit NAME(INTERNAL i) : Index(i) {} \
}

namespace GPU {
	// Handles to GPU objects
	// TODO: Reduces sizes - u16 most places?
	DECLARE_HANDLE(ConstantBufferID, u32);
	DECLARE_HANDLE(VertexBufferID, u32);
	DECLARE_HANDLE(IndexBufferID, u32);
	DECLARE_HANDLE(TextureID, u32);
	DECLARE_HANDLE(RenderTargetID, u32);
	DECLARE_HANDLE(StreamFormatID, u32);
	DECLARE_HANDLE(VertexShaderID, u32);
	DECLARE_HANDLE(PixelShaderID, u32);
	DECLARE_HANDLE(ProgramID, u32);
	DECLARE_HANDLE(InputAssemblerConfigID, u32);
	DECLARE_HANDLE(RasterStateID, u32);
	DECLARE_HANDLE(BlendStateID, u32);
	DECLARE_HANDLE(DepthStencilStateID, u32);
	DECLARE_HANDLE(ShaderResourceListID, u32);


	enum class ShaderType {
		Vertex,
		Pixel
	};

	enum class TextureFormat {
		RGB8,
		RGBA8,
	};

	static constexpr u8 MaxBoundShaderResources = 16;
	struct ShaderResourceListDesc {
		u8 StartSlot;
		mu::FixedArray<TextureID, MaxBoundShaderResources> Textures;
		// TODO: Buffer bindings
	};

	// Configuration for creating objects/commands
	enum class PrimitiveTopology {
		TriangleList,
		TriangleStrip,
	};

	enum class InputSemantic {
		Position,
		Color,
		Texcoord,
		Normal,
	};

	enum class ScalarType {
		Float,
	};

	// Stream format declaration
	static constexpr u8 MaxStreamSlots = 16;
	static constexpr u8 MaxStreamElements = 8;

	struct StreamElementDesc {
		ScalarType Type : 2;
		u8 Count : 2;
		InputSemantic Semantic : 6;
		u8 SemanticIndex : 6;

		StreamElementDesc() {};
		StreamElementDesc(ScalarType type, u8 count, InputSemantic semantic, u8 sem_index) {
			Type = type; Count = count; Semantic = semantic; SemanticIndex = sem_index;
		}
	};

	u32 GetStreamElementSize(const StreamElementDesc& element);

	struct StreamSlotDesc {
		mu::FixedArray<StreamElementDesc, MaxStreamElements> Elements;

		void Set(StreamElementDesc elem) {
			Elements.Empty();
			Elements.Add(elem);
		}
	};

	struct StreamFormatDesc {
		mu::FixedArray<StreamSlotDesc, MaxStreamSlots> Slots;

		StreamSlotDesc& AddSlot() {
			Slots.Add({});
			return Slots[Slots.Num() - 1];
		}
	};

	// Draw commands
	struct DrawCommand {
		u32 VertexOrIndexCount;
		PrimitiveTopology PrimTopology;
	};
	struct DrawPipelineSetup {
		RasterStateID			RasterState;
		BlendStateID			BlendState;
		DepthStencilStateID		DepthStencilState;
		ProgramID				Program;
		InputAssemblerConfigID	InputAssemblerConfig;
	};

	static constexpr size_t MaxBoundConstantBuffers = 4;
	static constexpr size_t MaxBoundResourceLists = 4;
	struct DrawBoundResources {
		mu::FixedArray<ConstantBufferID, MaxBoundConstantBuffers> ConstantBuffers;
		mu::FixedArray<ShaderResourceListID, MaxBoundResourceLists> ResourceLists;
	};
	struct DrawItem {
		DrawPipelineSetup	PipelineSetup;
		DrawBoundResources	BoundResources;
		DrawCommand			Command;
	};

	// Render pass
	static constexpr RenderTargetID BackBufferID = RenderTargetID{ u32_max };
	static constexpr u8 MaxRenderTargets = 16;
	struct RenderPass {
		mu::FixedArray<RenderTargetID, MaxRenderTargets> RenderTargets;
		mu::PointerRange<const DrawItem> DrawItems;
	};

	static_assert(std::is_trivially_destructible_v<DrawItem>, "RenderPass should be trivially destructible");
	static_assert(std::is_trivially_destructible_v<RenderPass>, "RenderPass should be trivially destructible");
}

struct GPUInterface {
	virtual ~GPUInterface() {}
	virtual void Init() = 0;
	virtual void Shutdown() = 0;
	virtual void RecreateSwapChain(void* hwnd, u32 width, u32 height) = 0;

	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual void SubmitPass(const GPU::RenderPass& pass) = 0;

	virtual GPU::StreamFormatID RegisterStreamFormat(const GPU::StreamFormatDesc& format) = 0;
	virtual GPU::InputAssemblerConfigID RegisterInputAssemblyConfig(GPU::StreamFormatID format, mu::PointerRange<const GPU::VertexBufferID> vertex_buffers, GPU::IndexBufferID index_buffer) = 0;

	virtual GPU::VertexShaderID CompileVertexShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) = 0;
	virtual GPU::PixelShaderID CompilePixelShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) = 0;
	virtual GPU::ProgramID LinkProgram(GPU::VertexShaderID vertex_shader, GPU::PixelShaderID pixel_shader) = 0;

	virtual GPU::ConstantBufferID CreateConstantBuffer(mu::PointerRange<const u8> data) = 0;
	virtual GPU::VertexBufferID CreateVertexBuffer(mu::PointerRange<const u8> data) = 0;

	virtual GPU::TextureID CreateTexture2D(u32 width, u32 height, GPU::TextureFormat format, mu::PointerRange<const u8> data) = 0;

	virtual GPU::ShaderResourceListID CreateShaderResourceList(const GPU::ShaderResourceListDesc& desc) = 0;
};

#undef DECLARE_HANDLE

#pragma warning(pop) 
