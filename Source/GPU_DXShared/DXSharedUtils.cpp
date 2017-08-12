#include "mu-core/Utils.h"

#include "DXSharedUtils.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

namespace DX {
	void CompileShaderHLSL(ID3DBlob** compiled_shader, const char* shader_model, const char* entry_point, const mu::PointerRange<const u8>& code) {
		DX::COMPtr<ID3DBlob> errors;

		if (!SUCCEEDED(D3DCompile(
			&code.Front(),
			code.Size(),
			nullptr,
			nullptr,
			nullptr,
			entry_point,
			shader_model,
			0,
			0,
			compiled_shader,
			errors.Replace()))) {
			const char* error_msg = (const char*)errors->GetBufferPointer();
			CHECKF(false, "Compile failed with error: ", error_msg);
		}
	}

	const char* GetHLSLTypeName(GPU::StreamElementDesc desc) {
		static const char* float_names[] = { "float", "float2", "float3", "float4" };
		static const char* uint_names[] = { "uint", "uint2", "uint3", "uint4" };
		static const char* int_names[] = { "int", "int2", "int3", "int4" };
		switch (desc.Type) {
		case GPU::ScalarType::Float: return float_names[desc.CountMinusOne];
		case GPU::ScalarType::U8: if (desc.Normalized) { return float_names[desc.CountMinusOne]; }
								  else { return uint_names[desc.CountMinusOne]; }
		}
		CHECK(false);
		return "";
	}
	const char* GetSemanticName(GPU::InputSemantic semantic) {
		const char* names[] = {
			"POSITION", "COLOR", "TEXCOORD", "NORMAL"
		};
		CHECK((u32)semantic < ArraySize(names));
		return names[(u32)semantic];
	}
	DXGI_FORMAT GetStreamElementFormat(GPU::StreamElementDesc desc) {
		switch (desc.Type) {
		case GPU::ScalarType::Float:
		{
			static const DXGI_FORMAT float_formats[] = {
				DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };
			return float_formats[desc.CountMinusOne];
		}
		case GPU::ScalarType::U8:
		{
			static const DXGI_FORMAT byte_formats[] = {
				DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R8G8_UINT, DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R8G8B8A8_UINT,
			};
			static const DXGI_FORMAT byte_formats_norm[] = {
				DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, 
			};
			return (desc.Normalized ? byte_formats_norm : byte_formats)[desc.CountMinusOne];
		}
		}
		return DXGI_FORMAT_UNKNOWN;
	}
}