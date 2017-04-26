#pragma once

#include "mu-core/PrimitiveTypes.h"
#include "mu-core/PointerRange.h"
#include "mu-core/FixedArray.h"

namespace GPUCommon {
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

	enum class ShaderType {
		Vertex,
		Pixel
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
	constexpr u8 MaxStreamSlots = 16;
	constexpr u8 MaxStreamElements = 8;

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

	constexpr size_t MaxBoundConstantBuffers = 4;
	struct DrawBoundResources {
		mu::FixedArray<ConstantBufferID, MaxBoundConstantBuffers> ConstantBuffers;
	};
	struct DrawItem {
		DrawPipelineSetup	PipelineSetup;
		DrawBoundResources	BoundResources;
		DrawCommand			Command;
	};

	// Render pass
	constexpr RenderTargetID BackBufferID = { u32_max };
	constexpr u8 MaxRenderTargets = 16;
	struct RenderPass {
		mu::FixedArray<RenderTargetID, MaxRenderTargets> RenderTargets;
		mu::PointerRange<const DrawItem> DrawItems;
	};
}