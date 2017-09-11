#pragma once

#include "mu-core/Debug.h"
#include "mu-core/PrimitiveTypes.h"
#include "mu-core/RefCountPtr.h"

#include "../GPUInterface.h"

#include <d3dcommon.h>
#include <dxgi1_4.h>

#define EnsureHR(expr) \
	do { \
		HRESULT hr =(expr); if(SUCCEEDED(hr)) {break;} \
		Assertf(SUCCEEDED(hr), #expr, "failed with error code ", hr); \
	} while(false);

namespace DX {
	template<typename OBJECT>
	struct COMRefCount {
		static void IncRef(OBJECT* o) {
			o->AddRef();
		}
		static void DecRef(OBJECT* o) {
			o->Release();
		}
	};

	template<typename COMObject>
	using COMPtr = RefCountPtr<COMObject, COMRefCount>;

	inline D3D_PRIMITIVE_TOPOLOGY CommonToDX(GPU::PrimitiveTopology pt) {
		static D3D_PRIMITIVE_TOPOLOGY list[] = {
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
		};
		return list[(i32)pt];
	}

	inline DXGI_FORMAT CommonToDX(GPU::TextureFormat format) {
		switch (format) {
		case GPU::TextureFormat::RGBA8:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		}
		Assert(false);
		return DXGI_FORMAT_UNKNOWN;
	}

	inline u32 CalcRowPitch(GPU::TextureFormat format, u32 width) {
		switch (format) {
		case GPU::TextureFormat::RGBA8:
			return 4 * width;
		}
		Assert(false);
		return 0;
	}

	struct VertexShaderInputElement {
		GPU::ScalarType Type : 2;
		u8 CountMinusOne : 2;
		GPU::InputSemantic Semantic : 6;
		u8 SemanticIndex : 6;
	};

	void CompileShaderHLSL(ID3DBlob** compiled_shader, const char* shader_model, const char* entry_point, const mu::PointerRange<const u8>& code);

	const char* GetHLSLTypeName(GPU::StreamElementDesc desc);
	const char* GetSemanticName(GPU::InputSemantic semantic);
	DXGI_FORMAT GetStreamElementFormat(GPU::StreamElementDesc desc);
}
