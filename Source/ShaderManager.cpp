#include "ShaderManager.h"

#include "mu-core/Array.h"
#include "mu-core/Debug.h"
#include "mu-core/Paths.h"
#include "mu-core/FileReader.h"

#include "imgui.h"

#include <filesystem>

using mu::Array;
using mu::String;

namespace fs = std::filesystem;
namespace dbg = mu::dbg;

// TODO: Directory watcher

namespace ShaderManagerInternal {
	String LoadShaderSourceCode(GPUInterface* GPU, GPU::ShaderType type, mu::PointerRange<const char> name) {
		const auto sub_dir = GPU->GetShaderSubdirectory();
		const auto extension = GPU->GetShaderFileExtension(type);

		fs::path exe_dir = mu::paths::GetExecutableDirectory();
		fs::path shader_dir = exe_dir / "../Shaders" / sub_dir.m_start;
		fs::path shader_path = shader_dir;
		shader_path /= name.m_start;
		shader_path.replace_extension(extension.m_start);
		shader_path.make_preferred();
		shader_path = shader_path.lexically_normal();

		dbg::Log("Shader path {}", shader_path.c_str());
		
		return mu::LoadFileToString(shader_path);
	}
}

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
	auto shader_source = ShaderManagerInternal::LoadShaderSourceCode(m_gpu, type, name);
	GPU::ShaderID id = m_gpu->CompileShader(type, shader_source.Bytes());
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
	auto shader_source = ShaderManagerInternal::LoadShaderSourceCode(m_gpu, shader.Type, shader.Name.Range());
	m_gpu->RecompileShader(id, shader.Type, shader_source.Bytes());
}

void ShaderManager::PushChangesToGPU() {
	// TODO
}