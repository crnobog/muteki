#pragma once

#include "mu-core/FixedArray.h"
#include "mu-core/PrimitiveTypes.h"
#include "mu-core/PointerRange.h"
#include "mu-core/String.h"
#include "mu-core/ZipRange.h"

#include "Rect.h"
#include "Vectors.h"

#pragma warning(push) 
#pragma warning(disable : 4100)

#define DECLARE_GPU_HANDLE(NAME, INTERNAL) \
struct NAME { \
	explicit operator size_t() const { return Index; } \
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
	DECLARE_GPU_HANDLE(ConstantBufferID, u32);
	DECLARE_GPU_HANDLE(VertexBufferID, u32);
	DECLARE_GPU_HANDLE(IndexBufferID, u32);
	DECLARE_GPU_HANDLE(TextureID, u32);
	DECLARE_GPU_HANDLE(RenderTargetID, u32);
	DECLARE_GPU_HANDLE(DepthTargetID, u32);
	DECLARE_GPU_HANDLE(ShaderID, u32);
	DECLARE_GPU_HANDLE(ProgramID, u32);
	DECLARE_GPU_HANDLE(ShaderResourceListID, u32);
	DECLARE_GPU_HANDLE(PipelineStateID, u32);
	DECLARE_GPU_HANDLE(FramebufferID, u32);

	// Limits
	static constexpr u8 MaxRenderTargets = 16;
	
	enum class ShaderType {
		Vertex,
		Pixel
	};

	struct ProgramDesc {
		ShaderID VertexShader;
		ShaderID PixelShader;
	};

	enum class TextureFormat {
		Unknown,
		R8, 
		RG8,
		RGBA8,

		NumFormats,
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
		LineList,
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
		StreamElementDesc(ScalarType type, u8 count, InputSemantic semantic, u8 sem_index, bool normalized = false) {
			Type = type; CountMinusOne = count - 1; Semantic = semantic; SemanticIndex = sem_index;
			Normalized = normalized;
		}
	};

	u32 GetStreamElementSize(const StreamElementDesc& element);

	struct StreamSlotDesc {
		mu::FixedArray<StreamElementDesc, MaxStreamElements> Elements;

		StreamSlotDesc(StreamElementDesc elem) {
			Elements.Add(elem);
		}
		StreamSlotDesc(std::initializer_list<StreamElementDesc> in_elems) {
			for (auto& elem : in_elems) { Elements.Add(elem); }
		}
	};

	struct StreamFormatDesc {
		mu::FixedArray<StreamSlotDesc, MaxStreamSlots> Slots;

		StreamFormatDesc& AddSlot(StreamElementDesc elem) {
			Slots.Emplace(elem);
			return *this;
		}

		StreamFormatDesc& AddSlot(std::initializer_list<StreamElementDesc> elems) {
			Slots.Emplace(elems);
			return *this;
		}
	};

	struct StreamSetup {
		mu::FixedArray<VertexBufferID, MaxStreamSlots> VertexBuffers;
		IndexBufferID IndexBuffer;
	};

	enum class FillMode {
		Solid,
		Wireframe,
	};
	enum class CullMode {
		None,
		Front,
		Back
	};
	enum class FrontFace {
		Clockwise,
		CounterClockwise,
	};

	struct RasterStateDesc {
		FillMode FillMode = FillMode::Solid;
		CullMode CullMode = CullMode::None;
		FrontFace FrontFace = FrontFace::Clockwise;
		i32 DepthBias = 0;
		float DepthBiasClamp = 0.0f;
		float SlopeScaledDepthBias = 0.0f;
		bool DepthClipEnable = false;
		bool ScissorEnable = false;
		bool MultisampleEnable = false;
		bool AntiAliasedLineEnable = false;
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

	enum class ComparisonFunc {
		Never,
		Less,
		Equal,
		LessThanOrEqual,
		Greater,
		NotEqual,
		GreaterThanOrEqual,
		Always,

		MAX,
	};

	enum class StencilOp {
		Keep,
		Zero,
		Replace,
		IncrementSaturated,
		DecrementSaturated,
		Invert,
		Increment,
		Decrement,

		MAX,
	};

	struct StencilOpDesc {
		StencilOp StencilFailOp = StencilOp::Keep;
		StencilOp StencilDepthFailOp = StencilOp::Keep;
		StencilOp StencilPassOp = StencilOp::Keep;
		ComparisonFunc StencilFunc = ComparisonFunc::Never;
	};

	struct DepthStencilStateDesc {
		bool DepthEnable = false;
		bool DepthWriteEnable = false;
		ComparisonFunc DepthComparisonFunc = ComparisonFunc::Never;
		bool StencilEnable = false;
		u8 StencilReadMask = 0;
		u8 StencilWriteMask = 0;
		StencilOpDesc StencilFrontFace = {};
		StencilOpDesc StencilBackFace = {};
	};

	enum class PrimitiveType : u8 {
		Triangle,
		Line
	};

	struct PipelineStateDesc {
		ProgramID				Program;
		RasterStateDesc			RasterState;
		BlendStateDesc			BlendState;
		DepthStencilStateDesc	DepthStencilState;
		StreamFormatDesc		StreamFormat;
		PrimitiveTopology		PrimitiveTopology;
	};

	struct FramebufferDesc {
		mu::FixedArray<RenderTargetID, GPU::MaxRenderTargets> RenderTargets;
		DepthTargetID DepthBuffer = {};
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
		const char*			Name = nullptr;
	};

	// Render pass
	static constexpr RenderTargetID BackBufferID = RenderTargetID{ u32_max };
	struct RenderPass {
		FramebufferID Framebuffer = {};
		Rect<u32> ClipRect = { 0, 0, 0, 0 }; // TODO: Document clip space?
		mu::PointerRange<const DrawItem> DrawItems;
		const float* DepthClearValue = nullptr;
		const char* Name = nullptr;
	};

	static_assert(std::is_trivially_destructible_v<DrawItem>, "DrawItem should be trivially destructible");
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
		for (auto[slot_a, slot_b] : Zip(a.Slots.Range(), b.Slots.Range())) {
			if (slot_a.Elements.Num() != slot_b.Elements.Num()) {
				return false;
			}
			for (auto[e_a, e_b] : Zip(slot_a.Elements.Range(), slot_b.Elements.Range())) {
				if (e_a != e_b) { return false; }
			}
		}
		return true;
	}

	inline bool operator==(const GPU::RasterStateDesc& a, const GPU::RasterStateDesc& b) {
		return a.CullMode == b.CullMode
			&& a.FillMode == b.FillMode
			&& a.FrontFace == b.FrontFace
			&& a.DepthBias == b.DepthBias
			&& a.DepthBiasClamp == b.DepthBiasClamp
			&& a.SlopeScaledDepthBias == b.SlopeScaledDepthBias
			&& a.ScissorEnable == b.ScissorEnable
			&& a.AntiAliasedLineEnable == b.AntiAliasedLineEnable
			&& a.MultisampleEnable == b.MultisampleEnable
			&& a.DepthClipEnable == b.DepthClipEnable;
	}
	inline bool operator!=(const GPU::RasterStateDesc& a, const GPU::RasterStateDesc& b) { return !(a == b); }

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

	inline bool operator==(const GPU::DepthStencilStateDesc& a, const GPU::DepthStencilStateDesc& b) {
		return true;
	}
	inline bool operator!=(const GPU::DepthStencilStateDesc& a, const GPU::DepthStencilStateDesc& b) {
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

	inline PrimitiveType PrimitiveTopologyToPrimitiveType(PrimitiveTopology topo)
	{
		switch(topo)
		{
		case PrimitiveTopology::LineList:
			return PrimitiveType::Line;
		default:
			return PrimitiveType::Triangle;
		}
	}
}

struct GPUFrameInterface {
	virtual ~GPUFrameInterface() {}
	// Returns a constant buffer that is only valid for the current frame
	//	e.g. may use a per-frame linear allocator on DX12
	virtual GPU::ConstantBufferID GetTemporaryConstantBuffer(mu::PointerRange<const u8> data) = 0;
	virtual GPU::VertexBufferID GetTemporaryVertexBuffer(mu::PointerRange<const u8> data, size_t alignment) = 0;
	virtual GPU::IndexBufferID GetTemporaryIndexBuffer(mu::PointerRange<const u8> data) = 0;
};

struct GPUInterface {
	virtual ~GPUInterface() {}
	virtual void Init(void* hwnd) = 0;
	virtual void Shutdown() = 0;
	virtual void CreateSwapChain(u32 width, u32 height) = 0;
	virtual void ResizeSwapChain(u32 width, u32 height) = 0;
	virtual Vector<u32, 2> GetSwapChainDimensions() = 0;

	virtual GPUFrameInterface* BeginFrame(Vec4 scene_clear_color) = 0;
	virtual void SubmitPass(const GPU::RenderPass& pass) = 0;
	virtual void EndFrame(GPUFrameInterface*) = 0;

	virtual mu::PointerRange<const char>	GetShaderSubdirectory() = 0;
	virtual mu::PointerRange<const char>	GetShaderFileExtension(GPU::ShaderType type) = 0;
	virtual GPU::ShaderID					CompileShader(GPU::ShaderType type, mu::PointerRange<const u8> source) = 0;
	virtual bool							RecompileShader(GPU::ShaderID id, GPU::ShaderType type, mu::PointerRange<const u8> source) = 0;
	virtual GPU::ProgramID					LinkProgram(GPU::ProgramDesc desc) = 0;

	virtual GPU::PipelineStateID	CreatePipelineState(const GPU::PipelineStateDesc& desc) = 0;
	virtual void					DestroyPipelineState(GPU::PipelineStateID id) = 0;

	virtual GPU::ConstantBufferID	CreateConstantBuffer(mu::PointerRange<const u8> data) = 0;
	virtual void					DestroyConstantBuffer(GPU::ConstantBufferID id) = 0;

	virtual GPU::VertexBufferID CreateVertexBuffer(mu::PointerRange<const u8> data) = 0;
	virtual void				DestroyVertexBuffer(GPU::VertexBufferID id) = 0;
	virtual GPU::IndexBufferID	CreateIndexBuffer(mu::PointerRange<const u8> data) = 0;
	virtual void				DestroyIndexBuffer(GPU::IndexBufferID id) = 0;

	virtual GPU::TextureID		CreateTexture2D(u32 width, u32 height, GPU::TextureFormat format, mu::PointerRange<const u8> data) = 0;
	virtual void				RecreateTexture2D(GPU::TextureID id, u32 width, u32 height, GPU::TextureFormat format, mu::PointerRange<const u8> data) = 0;

	virtual GPU::DepthTargetID	CreateDepthTarget(u32 width, u32 height) = 0;

	// TODO: Replace with descriptor set concept?
	virtual GPU::ShaderResourceListID CreateShaderResourceList(const GPU::ShaderResourceListDesc& desc) = 0;

	virtual GPU::FramebufferID	CreateFramebuffer(const GPU::FramebufferDesc& desc) = 0;
	virtual void				DestroyFramebuffer(GPU::FramebufferID) = 0;

	virtual const char* GetName() = 0;
};

#undef DECLARE_GPU_HANDLE

#pragma warning(pop) 
