#include "ShaderManager.h"

ShaderManager::ShaderManager(GPUInterface* GPU)
	: m_gpu(GPU)
{
}

GPU::ShaderID ShaderManager::CompilePixelShader(mu::PointerRange<const char> name) {
	return m_gpu->CompileShader(GPU::ShaderType::Pixel, name);
}

GPU::ShaderID ShaderManager::CompileVertexShader(mu::PointerRange<const char>  name) {
	return m_gpu->CompileShader(GPU::ShaderType::Vertex, name);
}

void ShaderManager::RecompileShader(GPU::ShaderID) {
}

void ShaderManager::PushChangesToGPU() {

}