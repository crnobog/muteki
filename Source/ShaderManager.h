#pragma once

#include "GPUInterface.h" // For ID types - those could be split into their own header?
#include "mu-core/PointerRange.h"

// Can be used to sit in the middle of a program and GPUInterface to allow updating of 
// shaders at runtime.
// When a shader is updated, all PipelineStateIDs that reference that shader will be recreated.

class ShaderManager
{
	GPUInterface* m_gpu;

public:
	ShaderManager(GPUInterface* GPU);

	// TODO: Combine and use enum to select shader stage?
	GPU::PixelShaderID CompilePixelShader(mu::PointerRange<const char> name);
	GPU::VertexShaderID CompileVertexShader(mu::PointerRange<const char> name);

	void RecompilePixelShader(GPU::PixelShaderID id);
	void RecompileVertexShader(GPU::VertexShaderID id);

	// TODO: Configure auto-watching shader files for changes

	void PushChangesToGPU();
};