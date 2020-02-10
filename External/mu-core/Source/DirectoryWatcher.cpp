#include "mu-core/Debug.h"
#include "mu-core/DirectoryWatcher.h"
#include "mu-core/WindowsWrapped.h"

#include <filesystem>

namespace mu {

	DirectoryWatcher::DirectoryWatcher()
		: m_handle(nullptr)
	{
	}

	void DirectoryWatcher::StartWatching(const fs::path& path, DirectoryWatchFlags flags) {
		if (m_handle) {
			FindCloseChangeNotification(m_handle);
			m_handle = nullptr;
		}

		DWORD filter = 0;
		if ((flags & DirectoryWatchFlags::Write) != DirectoryWatchFlags::None) {
			filter |= FILE_NOTIFY_CHANGE_LAST_WRITE;
		}

		HANDLE h = FindFirstChangeNotification(path.native().c_str(), true, filter);
		Assert(h);
		m_handle = h;
	}

	bool DirectoryWatcher::HasChanges() const {
		Assertf(m_handle, "DirectoryWatcher::HasChanges called before StartWatching");
		if (WAIT_OBJECT_0 == WaitForSingleObject(m_handle, 0)) {
			FindNextChangeNotification(m_handle);
			return true;
		}
		return false;
	}
}