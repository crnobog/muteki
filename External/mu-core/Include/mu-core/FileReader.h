#pragma once

#include "mu-core/Array.h"
#include "mu-core/String.h"

namespace std::filesystem
{
	class path;
}

namespace mu {
	namespace fs = std::filesystem;

	enum class IOResult
	{
		Success,
		FileLocked,
		FileNotFound,
		MiscError
	};

	class FileReader {
		void* m_handle = nullptr;
		IOResult m_error = IOResult::Success;

		FileReader(void* handle);
		FileReader(IOResult error);
	public:

		static FileReader Open(const fs::path& path);
		~FileReader();

		FileReader(FileReader&& other);
		FileReader& operator=(FileReader&& other);

		PointerRange<u8> Read(PointerRange<u8> dest_range);

		IOResult GetError() const;
		i64 GetFileSize() const;
		i64 GetFilePosition() const;
		i64 Remaining() const;
	};

	Array<uint8_t> LoadFileToArray(const fs::path& path);
	Array<uint8_t> LoadFileToArray(FileReader& reader);
	String LoadFileToString(const fs::path& path);
	String LoadFileToString(FileReader& reader);
}
