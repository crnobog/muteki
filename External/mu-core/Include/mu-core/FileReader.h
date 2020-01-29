#pragma once

#include "mu-core/Array.h"
#include "mu-core/String.h"

#include <filesystem>

namespace mu {
	namespace fs = std::filesystem;

	class FileReader {
		void* m_handle = nullptr;

		FileReader(void* handle);
	public:

		static FileReader Open(const fs::path& path);
		~FileReader();

		FileReader(FileReader&& other);
		FileReader& operator=(FileReader&& other);

		PointerRange<u8> Read(PointerRange<u8> dest_range);

		bool IsValidFile() const { return m_handle != nullptr; }
		i64 GetFileSize() const;
		i64 GetFilePosition() const;
		i64 Remaining() const;
	};

	Array<uint8_t> LoadFileToArray(const fs::path& path);
	String LoadFileToString(const fs::path& path);
	String LoadFileToString(FileReader& reader);
}
