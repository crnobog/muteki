#pragma once

#include "mu-core/PrimitiveTypes.h"
#include "mu-core/Ranges.h"

namespace GPUCommon {
// Handles to GPU objects
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
		u8 NumElements : 4;
		StreamElementDesc Elements[MaxStreamElements];

		StreamElementDesc& Element(u8 index) {
			if (index >= NumElements) {
				NumElements = index = 1;
			}
			return Elements[index];
		}

		void Set(StreamElementDesc elem) {
			NumElements = 1;
			Elements[0] = elem;
		}
	};

	struct StreamFormatDesc {
		StreamSlotDesc Slots[MaxStreamSlots];

		StreamSlotDesc& Slot(u32 index) { return Slots[index]; }
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
	struct DrawItem {
		DrawPipelineSetup	PipelineSetup;
		DrawCommand			Command;
	};

	// Render pass
	constexpr RenderTargetID BackBufferID = { u32_max };
	constexpr u8 MaxRenderTargets = 16;
	struct RenderPass {
		mu::PointerRange<const DrawItem> DrawItems;
		mu::PointerRange<const RenderTargetID> RenderTargets;
	};
}