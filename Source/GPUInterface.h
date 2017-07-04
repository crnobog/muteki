#pragma once

#include "mu-core/PrimitiveTypes.h"
#include "mu-core/PointerRange.h"
#include "mu-core/FixedArray.h"

#pragma warning(push) 
#pragma warning(disable : 4100)

struct GPUInterface {
	// Handles to GPU objects
	// TODO: Reduces sizes - u16 most places?
	struct ConstantBufferID {
		u32 Index = u32_max;
	};
	struct VertexBufferID {
		u32 Index = u32_max;
	};
	struct IndexBufferID {
		u32 Index = u32_max;
	};
	struct TextureID {
		u32 Index = u32_max;
	};
	struct StreamFormatID {
		u32 Index = u32_max;
	};
	struct VertexShaderID {
		u32 Index = u32_max;
	};
	struct PixelShaderID {
		u32 Index = u32_max;
	};
	struct ProgramID {
		u32 Index = u32_max;
	};
	struct RenderTargetID {
		u32 Index = u32_max;
	};
	struct InputAssemblerConfigID {
		u32 Index = u32_max;
	};
	struct RasterStateID {
		u32 Index = u32_max;
	};
	struct BlendStateID {
		u32 Index = u32_max;
	};
	struct DepthStencilStateID {
		u32 Index = u32_max;
	};
	struct ShaderResourceListID {
		u32 Index = u32_max;
	};

	enum class ShaderType {
		Vertex,
		Pixel
	};

	enum class TextureFormat {
		RGB8,
		RGBA8,
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

	static constexpr u8 MaxBoundShaderResources = 16;
	struct ShaderResourceListDesc {
		u8 StartSlot;
		mu::FixedArray<TextureID, MaxBoundShaderResources> Textures;
		// TODO: Buffer bindings
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

	static u32 GetStreamElementSize(const StreamElementDesc& element);

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
	static constexpr RenderTargetID BackBufferID = { u32_max };
	static constexpr u8 MaxRenderTargets = 16;
	struct RenderPass {
		mu::FixedArray<RenderTargetID, MaxRenderTargets> RenderTargets;
		mu::PointerRange<const DrawItem> DrawItems;
	};

	static_assert(std::is_trivially_destructible_v<DrawItem>, "RenderPass should be trivially destructible");
	static_assert(std::is_trivially_destructible_v<RenderPass>, "RenderPass should be trivially destructible");


	virtual ~GPUInterface() {}
	virtual void Init() = 0;
	virtual void Shutdown() = 0;
	virtual void RecreateSwapChain(void* hwnd, u32 width, u32 height) = 0;

	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;

	virtual void SubmitPass(const RenderPass& pass) = 0;

	virtual StreamFormatID RegisterStreamFormat(const StreamFormatDesc& format) = 0;
	virtual InputAssemblerConfigID RegisterInputAssemblyConfig(StreamFormatID format, mu::PointerRange<const VertexBufferID> vertex_buffers, IndexBufferID index_buffer) = 0;

	virtual VertexShaderID CompileVertexShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) = 0;
	virtual PixelShaderID CompilePixelShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) = 0;
	virtual ProgramID LinkProgram(VertexShaderID vertex_shader, PixelShaderID pixel_shader) = 0;

	virtual ConstantBufferID CreateConstantBuffer(mu::PointerRange<const u8> data) = 0;
	virtual VertexBufferID CreateVertexBuffer(mu::PointerRange<const u8> data) = 0;

	virtual TextureID CreateTexture2D(u32 width, u32 height, TextureFormat format, mu::PointerRange<const u8> data) = 0;

	virtual ShaderResourceListID CreateShaderResourceList(const ShaderResourceListDesc& desc) = 0;
};

#pragma warning(pop) 
