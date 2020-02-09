#pragma once

#include "GPUInterface.h"

#include "mu-core/DirectoryWatcher.h"
#include "mu-core/FileReader.h"
#include "mu-core/HashTable.h"
#include "mu-core/Timer.h"

#include <filesystem>

// TODO: Factor out common parent class?
class TextureManager
{
	struct Texture
	{
		mu::String Name;
		std::filesystem::file_time_type LastWriteTime;
	};

	GPUInterface* m_gpu;
	mu::HashTable<GPU::TextureID, Texture> m_textures;

	bool m_show_window = false;
	bool m_auto_reload = true;

	std::filesystem::path m_texture_dir;
	mu::DirectoryWatcher m_dir_watcher;

	struct TextureReloadRequest
	{
		GPU::TextureID ID;
		mu::Timer Timer;
	};
	mu::Array<TextureReloadRequest> m_textures_to_reload;

public:
	TextureManager(GPUInterface* GPU);
	bool* GetShowFlag() { return &m_show_window; }
	void DrawUI();

	GPU::TextureID LoadTexture(mu::PointerRange<const char> name);
	mu::IOResult ReloadTexture(GPU::TextureID id);

	void PushChangesToGPU();
};
