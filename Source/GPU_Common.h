#pragma once

#include "mu-core/PrimitiveTypes.h"

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
struct ShaderID {
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
}

// Configuration for creating objects/commands
enum class PrimitiveTopology {
	TriangleList,
	TriangleFan,
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

struct StreamElement {
	ScalarType Type : 2;
	u8 Count : 2;
	InputSemantic Semantic : 6;
	u8 SemanticIndex : 6;
};

struct StreamSlot {
	u8 Index : 4;
	u8 NumElements : 4;
	StreamElement Elements[MaxStreamElements];
};

struct StreamFormat {
	u8 NumSlots;
	StreamSlot Slots[MaxStreamSlots];
};


// Draw commands
struct DrawCommand {
	u32 VertexOrIndexCount;
	PrimitiveTopology PrimTopology;
};
struct DrawItem {
	InputAssemblerConfigID InputAssemblerConfig;
	DrawCommand Command;
};

// Render pass
constexpr RenderTargetID BackBufferID = { u32_max };
constexpr u8 MaxRenderTargets = 16;
struct RenderPass {
	DrawItem* DrawItems;
	u32	DrawItemCount;
	RenderTargetID RenderTargets[MaxRenderTargets];
	u8 NumRenderTargets;
};
