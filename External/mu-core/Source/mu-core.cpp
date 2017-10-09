#define MU_CORE_IMPL
#include "mu-core/mu-core.h"

#include <codecvt>

template<typename DERIVED, typename FROM_CHAR, typename TO_CHAR>
class StringConvRange_UTF8_UTF16_Base {
	std::codecvt_utf8_utf16<wchar_t> conv;
	std::mbstate_t mb{};
	mu::PointerRange<const FROM_CHAR> source;
	TO_CHAR buffer[256];
	bool empty = false;

	// Since we use the same converter but a different member function for each direction, we need these overloads but each subclass only uses one
	std::codecvt_base::result DoConv(const char* in_start, const char* in_end, const char*& in_next, wchar_t* out_start, wchar_t* out_end, wchar_t*& out_next) {
		return conv.in(mb, in_start, in_end, in_next, out_start, out_end, out_next);
	}
	std::codecvt_base::result DoConv(const wchar_t* in_start, const wchar_t* in_end, const wchar_t*& in_next, char* out_start, char* out_end, char*& out_next) {
		return conv.out(mb, in_start, in_end, in_next, out_start, out_end, out_next);
	}
public:
	StringConvRange_UTF8_UTF16_Base(mu::PointerRange<const FROM_CHAR> in) : source(in) {
		Advance();
	}
	bool IsEmpty() const { return empty; }
	const TO_CHAR* Front() const { return buffer; }
	void Advance() {
		if (source.IsEmpty()) {
			empty = true;
			return;
		}
		const FROM_CHAR* in_next = nullptr, *in_start = &source.Front();
		size_t in_size = source.Size();
		TO_CHAR* out_next = nullptr;
		std::codecvt_base::result res = DoConv(in_start, in_start + in_size, in_next,
			buffer, buffer + ArraySize(buffer) - 1, out_next);
		Assert(res != std::codecvt_base::noconv);

		// insert null byte so callers can use Front as a null-terminated c string
		*out_next = 0;

		source = { in_next, in_start + in_size };
		if (res == std::codecvt_base::error) {
			empty = true;
		}
	}
};

class StringConvRange_UTF8_UTF16 : public StringConvRange_UTF8_UTF16_Base<StringConvRange_UTF8_UTF16, char, wchar_t> {
public:
	StringConvRange_UTF8_UTF16(mu::PointerRange<const char> in) : StringConvRange_UTF8_UTF16_Base(in) {}
};
class StringConvRange_UTF16_UTF8 : public StringConvRange_UTF8_UTF16_Base<StringConvRange_UTF16_UTF8, wchar_t, char> {
public:
	StringConvRange_UTF16_UTF8(mu::PointerRange<const wchar_t> in) : StringConvRange_UTF8_UTF16_Base(in) {}
};

namespace mu {
	String WideStringToUTF8(PointerRange<const wchar_t> in) {
		String s;
		for (StringConvRange_UTF16_UTF8 conv{ in }; !conv.IsEmpty(); conv.Advance()) {
			s.Append(conv.Front());
		}

		return s;
	}
	String_T<wchar_t> UTF8StringToWide(PointerRange<const char> in) {
		String_T<wchar_t> s;
		for (StringConvRange_UTF8_UTF16 conv{ in }; !conv.IsEmpty(); conv.Advance()) {
			s.Append(conv.Front());
		}
		return s;
	}
}

namespace mu {
	struct DebugLogFormatOutput : public mu::IStringFormatOutput {
		wchar_t m_buffer[1024];
		wchar_t* m_cursor = m_buffer;

		void Flush() {
			if (m_cursor != m_buffer) {
				*m_cursor = 0; // null terminate before printing
				OutputDebugStringW(m_buffer);
			}
			m_cursor = m_buffer;
		}
		i64 RemainingSpace() { return ArraySize(m_buffer) - (m_cursor - m_buffer) - 1; /* Need 1 to put a null terminator */ };
		virtual void Write(StringFormatArg arg) override {
			switch (arg.m_type) {
			case StringFormatArgType::C_Str:
			{
				auto[s, len] = arg.m_c_str;

				// We don't know the internal size of the converter so flush and then copy wholesale 
				// - should the converter return a size tuple so we can adaptively avoid each flush without strlen?
				// - or we could expose the max size from the converter
				Flush();
				StringConvRange_UTF8_UTF16 conv{ Range(s, s + len) };
				for (; !conv.IsEmpty(); conv.Advance()) {
					OutputDebugStringW(conv.Front());
				}
			}
			break;
			case StringFormatArgType::Unsigned:
			{
				if (RemainingSpace() <= 20) {
					Flush();
				}
				int written = swprintf(m_cursor, RemainingSpace(), L"%llu", arg.m_uint);
				if (written >= 0) {
					m_cursor += written;
				}
			}
			break;
			default:
				throw std::runtime_error("Invalid argument type to log");
			}
		}
		virtual void Close() override {
			if (RemainingSpace() < 2) {
				Flush();
			}
			int written = swprintf(m_cursor, RemainingSpace(), L"\n");
			if (written > 0) {
				m_cursor += written;
			}
			Flush();
		}
	};
}

void mu::dbg::LogInternal(mu::dbg::details::LogLevel, const char* fmt, PointerRange<StringFormatArg> args) {
	mu::DebugLogFormatOutput output;
	Format(output, fmt, args);
	return;

	//wchar_t local_buf[1024];
	//wchar_t* local_cursor = local_buf;
	//auto LocalFlush = [&]() {
	//	if (local_cursor != local_buf) {
	//		*local_cursor = 0; // null terminate before printing
	//		OutputDebugStringW(local_buf);
	//	}
	//	local_cursor = local_buf;
	//};
	//auto RemainingSpace = [&]() { return ArraySize(local_buf) - (local_cursor - local_buf); };

	//auto WriteString = [&](const char* s, size_t len) {

	//	// We don't know the internal size of the converter so flush and then copy wholesale 
	//	// - should the converter return a size tuple so we can adaptively avoid each flush without strlen?
	//	// - or we could expose the max size from the converter
	//	LocalFlush();
	//	StringConvRange_UTF8_UTF16 conv{ Range(s, s + len) };
	//	for (; !conv.IsEmpty(); conv.Advance()) {
	//		OutputDebugStringW(conv.Front());
	//	}
	//};
	//auto WriteArg = [&](const StringFormatArg& arg) {
	//	switch (arg.m_type) {
	//	case StringFormatArgType::C_Str:
	//	{
	//		auto[s, size] = arg.m_c_str;
	//		WriteString(s, size);
	//	}
	//	break;
	//	case StringFormatArgType::Unsigned:
	//	{
	//		if (RemainingSpace() <= 20) {
	//			LocalFlush();
	//		}
	//		int written = swprintf(local_cursor, RemainingSpace(), L"%llu", arg.m_uint);
	//		if (written >= 0) {
	//			local_cursor += written;
	//		}
	//	}
	//	break;
	//	default:
	//		throw std::runtime_error("Invalid argument type to log");
	//	}
	//};

	//const char* block_begin = fmt;
	//const char* cursor = fmt;
	//for (; *cursor != '\0'; ++cursor) {
	//	if (*cursor != '{') { continue; }
	//	if (block_begin != cursor) {
	//		// Output bare string so far
	//		WriteString(block_begin, cursor - block_begin);
	//	}
	//	++cursor;
	//	if (*cursor == '{') {
	//		// Escaped char, begin a new block from here
	//		block_begin = cursor;
	//		continue;
	//	}
	//	else if (*cursor == '}') {
	//		// Empty format string specified, emit next unemitted format arg
	//		WriteArg(args.Front());
	//		args.Advance();
	//		block_begin = cursor + 1;
	//		continue; // will increment cursor to == block_begin
	//	}
	//	else {
	//		throw std::runtime_error("Invalid format string, only {} is implemented");
	//	}
	//}
	//if (block_begin != cursor) {
	//	WriteString(block_begin, cursor - block_begin);
	//}

	//int written = swprintf(local_cursor, RemainingSpace(), L"\n");
	//if (written > 0) {
	//	local_cursor += written;
	//}
	//LocalFlush();
}


FileReader::FileReader(void* handle) : m_handle(handle) {}

FileReader FileReader::Open(const char* path) {
	auto wide_path = mu::UTF8StringToWide(mu::Range(path, path + strlen(path) + 1));

	HANDLE handle = CreateFile(wide_path.GetRaw(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	return FileReader(handle);
}

mu::PointerRange<uint8_t>  FileReader::Read(mu::PointerRange<uint8_t> dest_range) {
	const uint32_t max_per_call = std::numeric_limits<uint32_t>::max();
	while (!dest_range.IsEmpty()) {
		uint32_t bytes_read = 0;
		const uint32_t call_bytes = max_per_call > dest_range.Size() ? uint32_t(dest_range.Size()) : max_per_call;
		if (!ReadFile(m_handle, static_cast<void*>(&dest_range.Front()), call_bytes, reinterpret_cast<LPDWORD>(&bytes_read), nullptr)) {
			throw std::runtime_error("ReadFile failed");
		}
		dest_range.AdvanceBy(bytes_read);
		if (bytes_read == 0) {
			break;
		}
	}
	return dest_range;
}

int64_t FileReader::GetFileSize() const {
	int64_t size = 0;
	GetFileSizeEx(m_handle, reinterpret_cast<PLARGE_INTEGER>(&size));
	return size;
}


mu::Array<u8> LoadFileToArray(const char* path, FileReadType type) {
	FileReader reader = FileReader::Open(path);
	size_t extra = type == FileReadType::Text ? 1 : 0;
	mu::Array<u8> arr;
	size_t size = reader.GetFileSize() + extra;
	arr.Reserve(size);
	arr.AddUninitialized(size);
	reader.Read(mu::Range(arr));
	arr[arr.Num() - 1] = 0;
	return std::move(arr);
}

namespace mu {
	namespace paths {
		PointerRange<const char> GetDirectory(PointerRange<const char> r) {
			auto end = FindLast(r, [](const char c) { return c == '/' || c == '\\'; });
			if (end.IsEmpty()) {
				// return empty, must be filename only?
				return PointerRange<const char>{ };
			}
			return PointerRange<const char>{ &r.Front(), &end.Front() + 1 };
		}

		PointerRange<const char> GetFilename(PointerRange<const char> r) {
			auto end = FindLast(r, [](const char c) { return c == '/' || c == '\\'; });
			if (end.IsEmpty()) {
				// return input, must be filename only?
				return r;
			}
			end.Advance();
			return end;
		}


		PointerRange<const char> GetExtension(PointerRange<const char> r) {
			auto start = FindLast(r, [](const char c) { return c == '/' || c == '\\'; });
			if (start.IsEmpty()) {
				start = r;
			}
			auto dot = FindLast(start, [](const char c) { return c == '.'; });
			if (!dot.IsEmpty()) {
				dot.Advance();
			}
			return dot;
		}

		PointerRange<const char> GetExecutablePath() {
			struct Initializer {
				String path;
				Initializer() {
					wchar_t local_buffer[1024];
					u32 size = GetModuleFileNameW(nullptr, local_buffer, (u32)ArraySize(local_buffer));
					if (size) {
						path = WideStringToUTF8(Range(local_buffer, size));
					}
				}
			};
			static Initializer init;
			return Range(init.path);
		}
		PointerRange<const char> GetExecutableDirectory() {
			return GetDirectory(GetExecutablePath());
		}

	}
}

#include "SpookyV2.h"

namespace mu {
	u64 HashMemory(void* data, size_t num_bytes) {
		return SpookyHash::Hash64(data, num_bytes, 0);
	}
}
