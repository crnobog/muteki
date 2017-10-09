#pragma once

#include "mu-core/Array.h"
#include "mu-core/String.h"

enum class FileReadType {
	Text,
	Binary,
};

mu::Array<uint8_t> LoadFileToArray(const char* path, FileReadType type);

class FileReader
{
	void* m_handle = nullptr;

	FileReader(void* handle);
public:

	static FileReader Open(const char* path);

	mu::PointerRange<u8> Read(mu::PointerRange<u8> dest_range);

	bool IsValidFile() const { return m_handle != nullptr; }
	i64 GetFileSize() const;
};
