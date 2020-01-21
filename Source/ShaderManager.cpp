#include "ShaderManager.h"

#include "imgui.h"

using mu::String;

ShaderManager::ShaderManager(GPUInterface* GPU)
	: m_gpu(GPU)
{
}

// TODO: Filter shaders
// TODO: Batch operations?
void ShaderManager::DrawUI() {
	if (!m_show_window) {
		return;
	}

	GPU::ShaderID recompile = {};
	ImVec2 size = { 400, 300 };
	if (ImGui::Begin("ShaderManager", &m_show_window, size, -1.0f, 0)) {
		ImGui::Columns(3);

		ImGui::Text("Name"); ImGui::NextColumn();
		ImGui::Text("Type"); ImGui::NextColumn();
		ImGui::Text("Tools"); ImGui::NextColumn();

		for (auto [id, shader] : m_shaders.Range()) {
			ImGui::PushID((void*)(size_t)id);
			ImGui::Text(shader.Name.GetRaw()); ImGui::NextColumn();
			switch (shader.Type) {
			case GPU::ShaderType::Vertex:
				ImGui::Text("Vertex"); ImGui::NextColumn(); break;
			case GPU::ShaderType::Pixel:
				ImGui::Text("Pixel"); ImGui::NextColumn(); break;
			}
			
			if (ImGui::Button("Recompile")) {
				recompile = id;
			}
			ImGui::NextColumn();
			ImGui::PopID();
		}
	}
	ImGui::End();

	if (recompile != GPU::ShaderID{}) {
		RecompileShader(recompile);
	}
}

GPU::ShaderID ShaderManager::CompileShader(GPU::ShaderType type, mu::PointerRange<const char> name) {
	GPU::ShaderID id = m_gpu->CompileShader(type, name);
	if (id == GPU::ShaderID{}) {
		return id; // Compile failure
	}

	m_shaders.Add(id, {
		String{name},
		type
	});

	return id;
}	

void ShaderManager::RecompileShader(GPU::ShaderID id) {
	if (!m_shaders.Contains(id)) {
		return;
	}

	const Shader& shader = m_shaders[id];
	m_gpu->RecompileShader(id, shader.Type, shader.Name.Range());
}

void ShaderManager::PushChangesToGPU() {
	// TODO
}