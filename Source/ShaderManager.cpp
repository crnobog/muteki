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

namespace ShaderManagerInternal {
	fs::path GetShaderPath(const fs::path& shader_dir, GPUInterface* GPU, GPU::ShaderType type, mu::PointerRange<const char> name) {
		const auto sub_dir = GPU->GetShaderSubdirectory();
		const auto extension = GPU->GetShaderFileExtension(type);

		fs::path shader_path = shader_dir;
		shader_path /= name.m_start;
		shader_path.replace_extension(extension.m_start);

		return shader_path;
	}

	String LoadShaderSourceCode(const fs::path& shader_dir, GPUInterface* GPU, GPU::ShaderType type, mu::PointerRange<const char> name) {
		return mu::LoadFileToString(GetShaderPath(shader_dir, GPU, type, name));
	}
}

ShaderManager::ShaderManager(GPUInterface* GPU)
	: m_gpu(GPU)
{
	fs::path exe_dir = mu::paths::GetExecutableDirectory();
	const auto sub_dir = GPU->GetShaderSubdirectory();
	m_shader_dir = (exe_dir / "../Shaders" / sub_dir.m_start).lexically_normal();
	
	m_dir_watcher.StartWatching(m_shader_dir);
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
	auto path = ShaderManagerInternal::GetShaderPath(m_shader_dir, m_gpu, type, name);
	auto write_time = fs::last_write_time(path);
	auto shader_source = mu::LoadFileToString(path); // TODO: Handle failure
	GPU::ShaderID id = m_gpu->CompileShader(type, shader_source.Bytes());
	if (id == GPU::ShaderID{}) {
		return id; // Compile failure
	}

	m_shaders.Add(id, {
		String{name},
		type,
		write_time
	});

	return id;
}	

bool ShaderManager::RecompileShader(GPU::ShaderID id) {
	if (!m_shaders.Contains(id)) {
		return false;
	}

	Shader& shader = m_shaders[id];
	auto path = ShaderManagerInternal::GetShaderPath(m_shader_dir, m_gpu, shader.Type, shader.Name.Range());
	if (auto file_reader = mu::FileReader::Open(path); file_reader.IsValidFile()) {
		auto shader_source = mu::LoadFileToString(file_reader);
		m_gpu->RecompileShader(id, shader.Type, shader_source.Bytes());
		// TODO: Handle compilation failure

		// Only update write time if we succeeded
		shader.LastWriteTime = fs::last_write_time(path);
		return true;
	}
	else {
		dbg::Log("Failed to open shader file {}, will retry later", path.c_str());
	}
	return false;
}

void ShaderManager::PushChangesToGPU() {
	Array<GPU::ShaderID> new_shaders_to_recompile;
	// TODO: Add timeouts here so we don't recompile too frequently
	for (GPU::ShaderID id : m_shaders_to_recompile) {
		Shader& shader = m_shaders[id];
		dbg::Log("Retrying recompile of shader {}", shader.Name);
		if (!RecompileShader(id)) {
			new_shaders_to_recompile.Add(id);
		}
	}

	if (m_dir_watcher.HasChanges()) {
		for (auto [id, shader] : m_shaders.Range()) {
			auto path = ShaderManagerInternal::GetShaderPath(m_shader_dir, m_gpu, shader.Type, shader.Name.Range());
			auto write_time = fs::last_write_time(path);
			if (write_time > shader.LastWriteTime) {
				dbg::Log("Shader {} has changed, recompiling", shader.Name);
				if (RecompileShader(id)) {
					new_shaders_to_recompile.Add(id);
				}
			}		
		}
	}
	
	m_shaders_to_recompile = std::move(new_shaders_to_recompile);
}