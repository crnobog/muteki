#pragma once

#include "GPUInterface.h" // For ID types - those could be split into their own header?

#include "mu-core/DirectoryWatcher.h"
#include "mu-core/HashTable.h"
#include "mu-core/PointerRange.h"
#include "mu-core/String.h"
#include "mu-core/Timer.h"

#include <filesystem>

// Can be used to sit in the middle of a program and GPUInterface to allow updating of 
// shaders at runtime.
// When a shader is updated, all PipelineStateIDs that reference that shader will be recreated.

enum class ShaderCompileResult
{
	Success,
	FileLocked,
	Failed,
};

class ShaderManager
{
	struct Shader
	{
		mu::String Name;
		GPU::ShaderType Type;
		std::filesystem::file_time_type LastWriteTime;
	};

	GPUInterface* m_gpu;
	mu::HashTable<GPU::ShaderID, Shader> m_shaders;

	bool m_show_window = false;
	bool m_auto_recompile = true;

	std::filesystem::path m_shader_dir;
	mu::DirectoryWatcher m_dir_watcher;

	// Shaders that have previously failed recompilation
	struct ShaderRecompileRequest
	{
		GPU::ShaderID ID;
		mu::Timer Timer;
	};
	mu::Array<ShaderRecompileRequest> m_shaders_to_recompile;

public:
	ShaderManager(GPUInterface* GPU);
	void ShowWindow() { m_show_window = true; }
	void DrawUI();

	GPU::ShaderID CompileShader(GPU::ShaderType type, mu::PointerRange<const char> name);
	ShaderCompileResult RecompileShader(GPU::ShaderID id);

	// TODO: Configure auto-watching shader files for changes
	void PushChangesToGPU();
};