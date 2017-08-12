#pragma once

#include "mu-core/PrimitiveTypes.h"
#include "mu-core/PointerRange.h"
#include "mu-core/FixedArray.h"

#include "Rect.h"
#include "Vectors.h"

#pragma warning(push) 
#pragma warning(disable : 4100)

#define DECLARE_HANDLE(NAME, INTERNAL) \
struct NAME { \
	explicit operator size_t() { return Index; } \
	constexpr NAME() = default; \
	constexpr explicit NAME(size_t i) : Index((INTERNAL)i) {} \
	constexpr explicit NAME(INTERNAL i) : Index(i) {} \
	bool operator==(NAME other) const { return Index == other.Index; } \
	bool operator!=(NAME other) const { return Index != other.Index; } \
private: \
	INTERNAL Index = INTERNAL##_max; \
}

namespace GPU {
	// Handles to GPU objects
	// TODO: Reduces sizes - u16 most places?
	DECLARE_HANDLE(ConstantBufferID, u32);
	DECLARE_HANDLE(VertexBufferID, u32);
	DECLARE_HANDLE(IndexBufferID, u32);
	DECLARE_HANDLE(TextureID, u32);
	DECLARE_HANDLE(RenderTargetID, u32);
	DECLARE_HANDLE(VertexShaderID, u32);
	DECLARE_HANDLE(PixelShaderID, u32);
	DECLARE_HANDLE(ProgramID, u32);
	DECLARE_HANDLE(RasterStateID, u32);
	DECLARE_HANDLE(DepthStencilStateID, u32);
	DECLARE_HANDLE(ShaderResourceListID, u32);
	DECLARE_HANDLE(PipelineStateID, u32);


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
	enum class PrimitiveTopology : u8 {
		TriangleList,
		TriangleStrip,
	};

	enum class InputSemantic : u8 {
		Position,
		Color,
		Texcoord,
		Normal,
	};

	enum class ScalarType : u8 {
		Float,
		U8,
	};

	// Stream format declaration
	static constexpr u8 MaxStreamSlots = 8; // Could be higher if necessary, or per-API.
	static constexpr u8 MaxStreamElements = 8;

	struct StreamElementDesc {
		ScalarType Type;
		u8 CountMinusOne;
		InputSemantic Semantic;
		u8 SemanticIndex;
		bool Normalized;

		StreamElementDesc() {};
		StreamElementDesc(ScalarType type, u8 count, InputSemantic semantic, u8 sem_index, bool normalized = false ) {
			Type = type; CountMinusOne = count - 1; Semantic = semantic; SemanticIndex = sem_index;
			Normalized = normalized;
		}
	};

	u32 GetStreamElementSize(const StreamElementDesc& element);

	struct StreamSlotDesc {
		mu::FixedArray<StreamElementDesc, MaxStreamElements> Elements;

		void Set(StreamElementDesc elem) {
			Elements.Empty();
			Elements.Add(elem);
		}

		StreamSlotDesc& Add(StreamElementDesc elem) {
			Elements.Add(elem);
			return *this;
		}
	};

	struct StreamFormatDesc {
		mu::FixedArray<StreamSlotDesc, MaxStreamSlots> Slots;

		StreamSlotDesc& AddSlot() {
			Slots.Add({});
			return Slots[Slots.Num() - 1];
		}
	};

	struct StreamSetup {
		mu::FixedArray<VertexBufferID, MaxStreamSlots> VertexBuffers;
		IndexBufferID IndexBuffer;
	};

	enum class BlendOp {
		Add,
		Subtract,
		ReverseSubtract,
		Min,
		Max,
	};
	enum class BlendValue {
		Zero,
		One,
		SourceColor,
		InverseSourceColor,
		SourceAlpha,
		InverseSourceAlpha,
		DestAlpha,
		InverseDestAlpha,
		DestColor,
		InverseDestColor,
	};

	struct BlendEquation {
		BlendValue Source;
		BlendOp	Op;
		BlendValue Dest;
	};

	struct BlendStateDesc {
		bool BlendEnable = false;
		bool AlphaToCoverageEnable = false;
		BlendEquation ColorBlend;
		BlendEquation AlphaBlend;
	};

	struct PipelineStateDesc {
		ProgramID				Program;
		RasterStateID			RasterState;
		BlendStateDesc			BlendState;
		DepthStencilStateID		DepthStencilState;
		StreamFormatDesc		StreamFormat;
	};

	// Draw commands
	struct DrawCommand {
		u32 VertexOrIndexCount;
		u32 IndexOffset;
		PrimitiveTopology PrimTopology;
	};

	static constexpr size_t MaxBoundConstantBuffers = 4;
	static constexpr size_t MaxBoundResourceLists = 4;
	struct DrawBoundResources {
		mu::FixedArray<ConstantBufferID, MaxBoundConstantBuffers> ConstantBuffers;
		mu::FixedArray<ShaderResourceListID, MaxBoundResourceLists> ResourceLists;
	};
	struct DrawItem {
		PipelineStateID		PipelineState;
		DrawBoundResources	BoundResources;
		DrawCommand			Command;
		StreamSetup			StreamSetup;
	};

	// Render pass
	static constexpr RenderTargetID BackBufferID = RenderTargetID{ u32_max };
	static constexpr u8 MaxRenderTargets = 16;
	struct RenderPass {
		Rectf ClipRect = { 0.0f, 0.0f, 0.0f, 0.0f }; // TODO: Document clip space?
		mu::FixedArray<RenderTargetID, MaxRenderTargets> RenderTargets;
		mu::PointerRange<const DrawItem> DrawItems;
	};

	static_assert(std::is_trivially_destructible_v<DrawItem>, "RenderPass should be trivially destructible");
	static_assert(std::is_trivially_destructible_v<RenderPass>, "RenderPass should be trivially destructible");

	inline bool operator==(const GPU::StreamElementDesc& a, const GPU::StreamElementDesc& b) {
		return a.CountMinusOne == b.CountMinusOne
			&& a.Semantic == b.Semantic
			&& a.SemanticIndex == b.SemanticIndex
			&& a.Type == b.Type;
	}
	inline bool operator!=(const GPU::StreamElementDesc& a, const GPU::StreamElementDesc& b) {
		return !(a == b);
	}
	inline bool operator==(const GPU::StreamFormatDesc& a, const GPU::StreamFormatDesc& b) {
		if (a.Slots.Num() != b.Slots.Num()) {
			return false;
		}
		for (std::tuple<const StreamSlotDesc&, const StreamSlotDesc&> p : Zip(a.Slots, b.Slots)) {
			const StreamSlotDesc& slot_a = std::get<0>(p);
			const StreamSlotDesc& slot_b = std::get<1>(p);
			if (slot_a.Elements.Num() != slot_b.Elements.Num()) {
				return false;
			}
			for (std::tuple<const StreamElementDesc&, const StreamElementDesc&> e : Zip(slot_a.Elements, slot_b.Elements)) {
				const StreamElementDesc& e_a = std::get<0>(e);
				const StreamElementDesc& e_b = std::get<1>(e);
				if (e_a != e_b) { return false; }
			}
		}
		return true;
	}

	inline bool operator==(const GPU::BlendEquation& a, const GPU::BlendEquation& b) {
		return a.Source == b.Source
			&& a.Op == b.Op
			&& a.Dest == b.Dest;
	}

	inline bool operator==(const GPU::BlendStateDesc& a, const GPU::BlendStateDesc& b) {
		return a.BlendEnable == b.BlendEnable
			&& a.AlphaToCoverageEnable == b.AlphaToCoverageEnable
			&& a.ColorBlend == b.ColorBlend
			&& a.AlphaBlend == b.AlphaBlend;
	}

	inline bool operator!=(const GPU::BlendStateDesc& a, const GPU::BlendStateDesc& b) {
		return !(a == b);
	}
	
	inline bool operator==(const GPU::PipelineStateDesc& a, const GPU::PipelineStateDesc& b) {
		return a.BlendState == b.BlendState
			&& a.DepthStencilState == b.DepthStencilState
			&& a.StreamFormat == b.StreamFormat
			&& a.Program == b.Program
			&& a.RasterState == b.RasterState;
	}

	inline bool operator!=(const GPU::PipelineStateDesc& a, const GPU::PipelineStateDesc& b) {
		return !(a == b);
	}
}

struct GPUFrameInterface {
	// Returns a constant buffer that is only valid for the current frame
	//	e.g. may use a per-frame linear allocator on DX12
	virtual GPU::ConstantBufferID GetTemporaryConstantBuffer(mu::PointerRange<const u8> data) = 0;
	virtual GPU::VertexBufferID GetTemporaryVertexBuffer(mu::PointerRange<const u8> data) = 0;
	virtual GPU::IndexBufferID GetTemporaryIndexBuffer(mu::PointerRange<const u8> data) = 0;
};

struct GPUInterface {
	virtual ~GPUInterface() {}
	virtual void Init() = 0;
	virtual void Shutdown() = 0;
	virtual void CreateSwapChain(void* hwnd, u32 width, u32 height) = 0;
	virtual void ResizeSwapChain(void* hwnd, u32 width, u32 height) = 0;
	virtual Vector<u32, 2> GetSwapChainDimensions() = 0;

	virtual GPUFrameInterface* BeginFrame() = 0;
	virtual void EndFrame(GPUFrameInterface*) = 0;

	virtual void SubmitPass(const GPU::RenderPass& pass) = 0;

	//virtual GPU::StreamFormatID RegisterStreamFormat(const GPU::StreamFormatDesc& format) = 0;
	//virtual GPU::InputAssemblerConfigID RegisterInputAssemblyConfig(GPU::StreamFormatID format, mu::PointerRange<const GPU::VertexBufferID> vertex_buffers, GPU::IndexBufferID index_buffer) = 0;

	virtual GPU::VertexShaderID CompileVertexShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) = 0;
	virtual GPU::PixelShaderID CompilePixelShaderHLSL(const char* entry_point, mu::PointerRange<const u8> code) = 0;
	virtual GPU::ProgramID LinkProgram(GPU::VertexShaderID vertex_shader, GPU::PixelShaderID pixel_shader) = 0;

	virtual GPU::PipelineStateID CreatePipelineState(const GPU::PipelineStateDesc& desc) = 0;
	virtual void DestroyPipelineState(GPU::PipelineStateID id) = 0;

	virtual GPU::ConstantBufferID CreateConstantBuffer(mu::PointerRange<const u8> data) = 0;
	virtual void DestroyConstantBuffer(GPU::ConstantBufferID id) = 0;

	virtual GPU::VertexBufferID CreateVertexBuffer(mu::PointerRange<const u8> data) = 0;
	virtual void DestroyVertexBuffer(GPU::VertexBufferID id) = 0;
	virtual GPU::IndexBufferID CreateIndexBuffer(mu::PointerRange<const u8> data) = 0;
	virtual void DestroyIndexBuffer(GPU::IndexBufferID id) = 0;

	virtual GPU::TextureID CreateTexture2D(u32 width, u32 height, GPU::TextureFormat format, mu::PointerRange<const u8> data) = 0;

	virtual GPU::ShaderResourceListID CreateShaderResourceList(const GPU::ShaderResourceListDesc& desc) = 0;
};

#undef DECLARE_HANDLE

#pragma warning(pop) 
