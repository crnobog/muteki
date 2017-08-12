
#include "DX11Utils.h"
#include "../GPU_DXShared/DXSharedUtils.h"
#include "../GPUInterface.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

namespace DX11Util {
	DX::VertexShaderInputElement ParseInputParameter(D3D11_SIGNATURE_PARAMETER_DESC& input_param) {
		std::tuple<GPU::InputSemantic, const char*> table[] = {
			{ GPU::InputSemantic::Position, "POSITION" },
			{ GPU::InputSemantic::Color, "COLOR" },
			{ GPU::InputSemantic::Texcoord, "TEXCOORD" },
			{ GPU::InputSemantic::Normal, "NORMAL" },
		};
		auto found = mu::Find(mu::Range(table), [&](const std::tuple<GPU::InputSemantic, const char*>& sem) {
			return strcmp(std::get<1>(sem), input_param.SemanticName) == 0;
		});
		CHECK(!found.IsEmpty());
		DX::VertexShaderInputElement out_elem;
		out_elem.Semantic = std::get<0>(found.Front());
		out_elem.SemanticIndex = input_param.SemanticIndex;
		out_elem.Type = GPU::ScalarType::Float;
		out_elem.CountMinusOne = 3;
		return out_elem;
	}
}