#pragma once

#include "mu-core/PrimitiveTypes.h"

#include <filesystem>

namespace mu {
	namespace fs = std::filesystem;

	enum class DirectoryWatchFlags : u8 {
		None = 0x0,
		Write = 0x1,
	};
	inline DirectoryWatchFlags operator|(DirectoryWatchFlags a, DirectoryWatchFlags b) {
		using U = std::underlying_type_t<DirectoryWatchFlags>;
		return static_cast<DirectoryWatchFlags>(static_cast<U>(a) | static_cast<U>(b));
	}
	inline DirectoryWatchFlags operator&(DirectoryWatchFlags a, DirectoryWatchFlags b) {
		using U = std::underlying_type_t<DirectoryWatchFlags>;
		return static_cast<DirectoryWatchFlags>(static_cast<U>(a) & static_cast<U>(b));
	}

	class DirectoryWatcher {
		void* m_handle;
		fs::path m_path;

	public:
		DirectoryWatcher();
		void StartWatching(fs::path path, DirectoryWatchFlags flags = DirectoryWatchFlags::Write);
		bool HasChanges() const;
	};
}