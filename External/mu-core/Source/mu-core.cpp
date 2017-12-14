#define MU_CORE_IMPL
#include "mu-core/mu-core.h"
#include "mu-core/String.h"

#include <codecvt>

namespace mu {
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

	void mu::dbg::LogInternal(mu::dbg::details::LogLevel, const char* fmt, PointerRange<StringFormatArg> args) {
		mu::DebugLogFormatOutput output;
		Format(output, fmt, args);
	}


	FileReader::FileReader(void* handle) : m_handle(handle) {}

	FileReader::~FileReader() {
		CloseHandle(m_handle);
	}

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

	i64 FileReader::GetFileSize() const {
		i64 size = 0;
		GetFileSizeEx(m_handle, reinterpret_cast<PLARGE_INTEGER>(&size));
		return size;
	}

	i64 FileReader::GetFilePosition() const {
		LARGE_INTEGER zero;
		zero.QuadPart = 0;
		i64 pos = -1;
		SetFilePointerEx(m_handle, zero, reinterpret_cast<PLARGE_INTEGER>(&pos), FILE_CURRENT);
		return pos;
	}

	i64 FileReader::Remaining() const {
		i64 size = GetFileSize();
		i64 pos = GetFilePosition();
		return size - pos;
	}

	mu::Array<u8> LoadFileToArray(const char* path) {
		FileReader reader = FileReader::Open(path);
		mu::Array<u8> arr;
		size_t size = reader.GetFileSize();
		arr.Reserve(size);
		reader.Read(arr.AddUninitialized(size));
		return arr;
	}

	String LoadFileToString(const char* path) {
		FileReader reader = FileReader::Open(path);
		if (reader.GetFileSize() < 3) {
			u8 b[3] = { 0, };
			reader.Read(Range(b));
			return String(b);
		}

		u8 BOM[3] = { 0, };
		reader.Read(Range(BOM));
		String s;
		if (BOM[0] != 0xEF || BOM[1] != 0xBB || BOM[2] != 0xBF) {
			// Add those 3 bytes to the string if they aren't a byte order mark
			s = String(BOM);
		}

		PointerRange<char> c = s.AddUninitialized(reader.Remaining());
		PointerRange<u8> r = c.Bytes();
		reader.Read(r);
		return s;
	}
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
