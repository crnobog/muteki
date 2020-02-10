#pragma once

#include "mu-core/PrimitiveTypes.h"

namespace std::filesystem
{
	class path;
}

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

	public:
		DirectoryWatcher();
		void StartWatching(const fs::path& path, DirectoryWatchFlags flags = DirectoryWatchFlags::Write);
		bool HasChanges() const;
	};
}