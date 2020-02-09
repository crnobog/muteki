#include "TextureManager.h"

#include "mu-core/Array.h"
#include "mu-core/Debug.h"
#include "mu-core/Paths.h"
#include "mu-core/FileReader.h"

#include "imgui.h"

#define STBI_NO_STDIO 
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x) Assert(x)
#include "stb_image.h"

#include <filesystem>

using mu::Array;
using mu::String;

namespace fs = std::filesystem;
namespace dbg = mu::dbg;

namespace TextureManagerInternal {
	fs::path GetTexturePath(const fs::path& texture_dir, mu::PointerRange<const char> name) {
		fs::path path = texture_dir;
		path /= name.m_start;
		return path;
	}

	fs::path ComputeTextureDirectory() {
		fs::path exe_dir = mu::paths::GetExecutableDirectory();
		return (exe_dir / "../Textures").lexically_normal();
	}
}

TextureManager::TextureManager(GPUInterface* GPU)
	: m_gpu(GPU)
	, m_texture_dir(TextureManagerInternal::ComputeTextureDirectory())
{
	m_dir_watcher.StartWatching(m_texture_dir);
}

// TODO: Filter 
// TODO: Batch operations?
void TextureManager::DrawUI() {
	if (!m_show_window) {
		return;
	}

	GPU::TextureID reload_id = {};
	ImVec2 size = { 350, 200 };
	ImGui::SetNextWindowPos({ 400, 60 }, ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
	if (ImGui::Begin("TextureManager", &m_show_window)) {
		ImGui::Checkbox("Auto-recompile", &m_auto_reload);

		ImGui::Separator();
		ImGui::Columns(3);

		ImGui::Text("Name"); ImGui::NextColumn();
		ImGui::Text("Type"); ImGui::NextColumn();
		ImGui::Text("Tools"); ImGui::NextColumn();
		ImGui::Separator();

		for (auto [id, texture] : m_textures.Range()) {
			ImGui::PushID((void*)(size_t)id);
			ImGui::Text(texture.Name.GetRaw()); ImGui::NextColumn();

			if (ImGui::Button("Reload")) {
				reload_id = id;
			}
			ImGui::NextColumn();
			ImGui::PopID();
		}
	}
	ImGui::End();

	if (reload_id != GPU::TextureID{}) {
		ReloadTexture(reload_id);
	}
}

struct TextureData {
	mu::IOResult result;
	mu::PointerRange<const u8> texture_data;
	u32 width, height;
	GPU::TextureFormat format;
	fs::file_time_type write_time;
};

TextureData LoadTexture(const fs::path dir, mu::PointerRange<const char> name) {
	auto path = TextureManagerInternal::GetTexturePath(dir, name);
	Assert(fs::exists(path));
	auto write_time = fs::last_write_time(path);
	auto data = mu::LoadFileToArray(path); // TODO: Handle failure
	// TODO: Make sure data is smaller than 2gb

	u32 width, height;
	GPU::TextureFormat format = GPU::TextureFormat::Unknown;
	mu::PointerRange<const u8> final_data;
	{
		i32 x, y, comp;
		stbi_info_from_memory(data.Data(), (i32)data.Num(), &x, &y, &comp);
		i32 req_comp = comp == 3 ? 4 : comp;
		const u8* raw = (const u8*)stbi_load_from_memory(data.Data(), (i32)data.Num(), &x, &y, &comp, req_comp);
		final_data = mu::PointerRange<const u8>{ raw, raw + x * y * req_comp };
		switch (req_comp)
		{
		case 1: format = GPU::TextureFormat::R8;
			break;
		case 2: format = GPU::TextureFormat::RG8;
			break;
		case 4: format = GPU::TextureFormat::RGBA8;
			break;
		default:
			Assert(false);
		}
		width = x;
		height = y;
	}
	return { mu::IOResult::Success, final_data, width, height, format, write_time };
}

GPU::TextureID TextureManager::LoadTexture(mu::PointerRange<const char> name) {
	TextureData data = ::LoadTexture(m_texture_dir, name);
	GPU::TextureID id = m_gpu->CreateTexture2D(data.width, data.height, data.format, data.texture_data);
	if (id == GPU::TextureID{}) {
		return id; // Compile failure
	}

	m_textures.Add(id, {
		String{name},
		data.write_time
	});

	return id;
}

mu::IOResult TextureManager::ReloadTexture(GPU::TextureID id) {
	if (!m_textures.Contains(id)) {
		return mu::IOResult::MiscError;
	}

	Texture& texture = m_textures[id];
	TextureData data = ::LoadTexture(m_texture_dir, texture.Name.Range());
	if (auto err = data.result; err != mu::IOResult::Success) {
		if (err == mu::IOResult::FileLocked) {
			return mu::IOResult::FileLocked;
		}
		else {
			return err;
		}
	}

	m_gpu->RecreateTexture2D(id, data.width, data.height, data.format, data.texture_data);

	texture.LastWriteTime = data.write_time;
	return mu::IOResult::Success;
}

void TextureManager::PushChangesToGPU() {
	if (!m_auto_reload) {
		return;
	}

	Array<TextureReloadRequest> new_textures_to_reload;
	// TODO: Add timeouts here so we don't recompile too frequently
	for (TextureReloadRequest request : m_textures_to_reload) {
		if (request.Timer.GetElapsedTimeSeconds() < 0.5f) {
			// We do insertions in order so there can't be any older requests than this
			new_textures_to_reload.Add(request);
			continue;
		}

		GPU::TextureID id = request.ID;
		Texture& texture = m_textures[id];
		dbg::Log("Retrying recompile of texture {}", texture.Name);
		switch (ReloadTexture(id)) {
		case mu::IOResult::FileLocked:
			new_textures_to_reload.Emplace(id, mu::Timer{});
			break;
		default:
			break;
		}
	}

	if (m_dir_watcher.HasChanges()) {
		for (auto [id, texture] : m_textures.Range()) {
			auto path = TextureManagerInternal::GetTexturePath(m_texture_dir, texture.Name.Range());
			auto write_time = fs::last_write_time(path);
			if (write_time <= texture.LastWriteTime) {
				continue;
			}

			dbg::Log("Shader {} has changed, recompiling", texture.Name);
			switch (ReloadTexture(id)) {
			case mu::IOResult::FileLocked:
				dbg::Log("Texture file {} locked, will retry later", texture.Name);
				new_textures_to_reload.Emplace(id, mu::Timer{});
				break;
			default:
				break;
			}
		}
	}

	m_textures_to_reload = std::move(new_textures_to_reload);
}