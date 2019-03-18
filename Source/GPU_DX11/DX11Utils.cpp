
#include "DX11Utils.h"
#include "../GPU_DXShared/DXSharedUtils.h"
#include "../GPUInterface.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

namespace DX11Util {
	bool ParseInputParameter(D3D11_SIGNATURE_PARAMETER_DESC& input_param, DX::VertexShaderInputElement& out_elem) {
		const char* skip_table[] = {
			"SV_VertexID",
		};
		auto found_skip = mu::Find(mu::Range(skip_table), [&](const char* sem) {
			return strcmp(sem, input_param.SemanticName) == 0;
		});
		if (!found_skip.IsEmpty()) {
			return false;
		}

		std::tuple<GPU::InputSemantic, const char*> table[] = {
			{ GPU::InputSemantic::Position, "POSITION" },
			{ GPU::InputSemantic::Color, "COLOR" },
			{ GPU::InputSemantic::Texcoord, "TEXCOORD" },
			{ GPU::InputSemantic::Normal, "NORMAL" },
		};
		auto found = mu::Find(mu::Range(table), [&](const std::tuple<GPU::InputSemantic, const char*>& sem) {
			return strcmp(std::get<1>(sem), input_param.SemanticName) == 0;
		});
		Assert(!found.IsEmpty());
		out_elem.Semantic = std::get<0>(found.Front());
		out_elem.SemanticIndex = input_param.SemanticIndex;
		out_elem.Type = GPU::ScalarType::Float;
		out_elem.CountMinusOne = 3;
		return true;
	}
}