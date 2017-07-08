#pragma once

#include "GPU_DX11.h"
#include "../GPU_DXShared/DXSharedUtils.h"
#include "../GPUInterface.h"

#include <d3d11.h>
#include <d3d11shader.h>

namespace DX11Util {
	DX::VertexShaderInputElement ParseInputParameter(D3D11_SIGNATURE_PARAMETER_DESC& input_param);
};