#include "ShaderManager.h"

ShaderManager::ShaderManager(GPUInterface* GPU)
	: m_gpu(GPU)
{
}

GPU::PixelShaderID ShaderManager::CompilePixelShader(mu::PointerRange<const char> name) {
	return m_gpu->CompilePixelShader(name);
}

GPU::VertexShaderID ShaderManager::CompileVertexShader(mu::PointerRange<const char>  name) {
	return m_gpu->CompileVertexShader(name);
}

void ShaderManager::RecompilePixelShader(GPU::PixelShaderID) {
}

void ShaderManager::RecompileVertexShader(GPU::VertexShaderID) {
}

void ShaderManager::PushChangesToGPU() {

}