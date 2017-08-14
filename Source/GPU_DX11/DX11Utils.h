#pragma once

#include "GPU_DX11.h"
#include "../GPU_DXShared/DXSharedUtils.h"
#include "../GPUInterface.h"

#include <d3d11.h>
#include <d3d11shader.h>

namespace DX11Util {
	DX::VertexShaderInputElement ParseInputParameter(D3D11_SIGNATURE_PARAMETER_DESC& input_param);

	inline D3D11_BLEND_OP CommonToDX11(GPU::BlendOp op) {
		switch (op) {
		case GPU::BlendOp::Add:				return D3D11_BLEND_OP_ADD;
		case GPU::BlendOp::Subtract:		return D3D11_BLEND_OP_SUBTRACT;
		case GPU::BlendOp::ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
		case GPU::BlendOp::Min:				return D3D11_BLEND_OP_MIN;
		case GPU::BlendOp::Max:				return D3D11_BLEND_OP_MAX;
		}
		CHECK(false);
		return D3D11_BLEND_OP_ADD;
	}

	inline D3D11_BLEND CommonToDX11(GPU::BlendValue val) {
		switch (val) {
		case GPU::BlendValue::Zero:					return D3D11_BLEND_ZERO;
		case GPU::BlendValue::One:					return D3D11_BLEND_ONE;
		case GPU::BlendValue::SourceColor:			return D3D11_BLEND_SRC_COLOR;
		case GPU::BlendValue::InverseSourceColor:	return D3D11_BLEND_INV_SRC_COLOR;
		case GPU::BlendValue::SourceAlpha:			return D3D11_BLEND_SRC_ALPHA;
		case GPU::BlendValue::InverseSourceAlpha:	return D3D11_BLEND_INV_SRC_ALPHA;
		case GPU::BlendValue::DestAlpha:			return D3D11_BLEND_DEST_ALPHA;
		case GPU::BlendValue::InverseDestAlpha:		return D3D11_BLEND_INV_DEST_ALPHA;
		case GPU::BlendValue::DestColor:			return D3D11_BLEND_DEST_COLOR;
		case GPU::BlendValue::InverseDestColor:		return D3D11_BLEND_INV_DEST_COLOR;
		}
		CHECK(false);
		return D3D11_BLEND_ZERO;
	}

	inline D3D11_FILL_MODE CommonToDX11(GPU::FillMode mode) {
		switch (mode) {
		case GPU::FillMode::Wireframe: return D3D11_FILL_WIREFRAME;
		case GPU::FillMode::Solid: return D3D11_FILL_SOLID;
		}
		CHECK(false);
		return D3D11_FILL_WIREFRAME;
	}

	inline D3D11_CULL_MODE CommonToDX11(GPU::CullMode mode) {
		switch (mode) {
		case GPU::CullMode::None: return D3D11_CULL_NONE;
		case GPU::CullMode::Front: return D3D11_CULL_FRONT;
		case GPU::CullMode::Back: return D3D11_CULL_BACK;
		}
		CHECK(false);
		return D3D11_CULL_NONE;
	}
};