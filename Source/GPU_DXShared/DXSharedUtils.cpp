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
		switch (desc.Type) {
		case GPU::ScalarType::Float:
			static const char* names[] = { "float", "float2", "float3", "float4" };
			CHECK(desc.Count < ArraySize(names));
			return names[desc.Count];
		}
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
			static const DXGI_FORMAT float_formats[] = {
				DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };
			CHECK(desc.Count > 0 && desc.Count < 5);
			return float_formats[desc.Count-1];
		}
		return DXGI_FORMAT_UNKNOWN;
	}
}