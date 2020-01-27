#define MU_CORE_IMPL
#include "mu-core/mu-core.h"
#include "mu-core/String.h"

#include <codecvt>

namespace mu {
	template String_T<char>;

	template PointerRange<char>;
	template PointerRange<const char>;
	template PointerRange<u8>;
	template PointerRange<const u8>;

	template<typename DERIVED, typename FROM_CHAR, typename TO_CHAR>
	class StringConvRange_UTF8_UTF16_Base {
		mu::PointerRange<const FROM_CHAR> source;
		TO_CHAR buffer[256];
		bool empty = false;

		// Decode from utf16 to a single unicode code point
		size_t Decode(u32& code_point, const wchar_t* in_start, size_t in_left) {
			if (in_start[0] < 0xD800) {
				code_point = in_start[0];
				return 1;
			}
			else if (in_start[0] < 0xE000) {
				if (in_left < 2) { return 0; }
				if ((in_start[0] & 0b1111110000000000) != 0b1101100000000000) { return 0; }
				if ((in_start[1] & 0b1111110000000000) != 0b1101110000000000) { return 0; }

				code_point = (in_start[0] & 0b0000001111111111) << 10;
				code_point |= (in_start[1] & 0b0000001111111111);
				code_point += 0x10000;
				return 2;
			}
			else {
				code_point = in_start[0];
				return 1;
			}
		}
		// Decode from utf8 to a single unicode code point
		size_t Decode(u32& code_point, const char* in_start, size_t in_left) {
			if ((in_start[0] & 0b10000000) == 0) {
				code_point = in_start[0] & 0b01111111;
				return 1;
			}
			else if ((in_start[0] & 0b11100000) == 0b11000000) {
				if (in_left < 2) { return 0; }
				code_point = (in_start[0] & 0b00011111) << 6;
				code_point = (in_start[1] | 0b00111111);
				return 2;
			}
			else if ((in_start[0] & 0b11110000) == 0b11100000) {
				if (in_left < 3) { return 0; }
				code_point = (in_start[0] & 0b00001111) << 12;
				code_point |= (in_start[1] | 0b00111111) << 6;
				code_point |= (in_start[2] | 0b00111111);
				return 3;
			}
			else if ((in_start[0] & 0b11111000) == 0b11110000) {
				if (in_left < 4) { return 0; }
				code_point = (in_start[0] & 0b00000111) << 18;
				code_point |= (in_start[1] | 0b00111111) << 6;
				code_point |= (in_start[2] | 0b00111111) << 6;
				code_point |= (in_start[3] | 0b00111111);
				return 4;
			}
			else {
				return 0; // Invalid
			}
		}

		// Encode a single code point to utf16
		size_t Encode(u32 code_point, wchar_t* in_start) {
			if (code_point < 0xD800) {
				in_start[0] = (wchar_t)code_point;
				return 1;
			}
			else if (code_point < 0xE000) {
				code_point -= 0x10000;
				in_start[0] = (wchar_t)(code_point >> 10) + 0xD800;
				in_start[1] = (wchar_t)(code_point & 0b1111111111) + 0xDC00;
				return 2;
			}
			else {
				in_start[0] = (wchar_t)code_point;
				return 1;
			}
		}

		// Encode a single code point to utf8
		size_t Encode(u32 code_point, char* in_start) {
			if (code_point < 0x80) {
				in_start[0] = (char)code_point;
				return 1;
			}
			else if (code_point < 0x800) {
				in_start[0] = 0b11000000 | (char)(code_point >> 6);
				in_start[1] = 0b10000000 | (char)(code_point & 0b00111111);
				return 2;
			}
			else if (code_point < 0x10000) {
				in_start[0] = 0b11100000 | (char)(code_point >> 12);
				in_start[1] = 0b10000000 | (char)((code_point >> 6) & 0b00111111);
				in_start[2] = 0b10000000 | (char)(code_point & 0b00111111);
				return 3;
			}
			else {
				in_start[0] = 0b11110000 | (char)(code_point >> 18);
				in_start[1] = 0b10000000 | (char)((code_point >> 12) & 0b00111111);
				in_start[2] = 0b10000000 | (char)((code_point >> 6) & 0b00111111);
				in_start[3] = 0b10000000 | (char)(code_point & 0b00111111);
				return 4;
			}
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

			TO_CHAR* out_next = buffer;
			size_t out_left = sizeof(buffer) / sizeof(buffer[0]);
			size_t min_for_decode = 0;
			bool error = false;
			do {
				u32 code_point = 0;
				const FROM_CHAR* in_start = &source.Front();
				size_t in_left = source.Size();
				size_t consumed = Decode(code_point, in_start, in_left);
				if (consumed == 0) {
					// Failed to decode a code point
					error = true;
					break;
				}
				source.AdvanceBy(consumed);

				size_t num_encoded = Encode(code_point, out_next);
				out_next += num_encoded;
				out_left -= num_encoded;
			} while (!source.IsEmpty() && out_left > min_for_decode);
			*out_next = 0;
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
			case StringFormatArgType::Wide_C_Str:
			{
				// TODO: Currently assumes string is 0 terminated, check this and copy to temp storage if necessary
				auto [s, len] = arg.m_w_c_str;
				Assert(s[len] == '\0');

				OutputDebugStringW(s);
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
			case StringFormatArgType::Signed:
			{
				if (RemainingSpace() <= 20) {
					Flush();
				}
				int written = swprintf(m_cursor, RemainingSpace(), L"%lld", arg.m_int);
				if (written >= 0) {
					m_cursor += written;
				}
			}
			break;
			case StringFormatArgType::Double:
			{
				if (RemainingSpace() <= 21) {
					Flush();
				}
				int written = swprintf(m_cursor, RemainingSpace(), L"%f", arg.m_double);
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
		FormatRange(output, fmt, args);
	}


	FileReader::FileReader(void* handle) : m_handle(handle) {}

	FileReader::~FileReader() {
		CloseHandle(m_handle);
	}

	FileReader FileReader::Open(const char* path) {
		auto wide_path = mu::UTF8StringToWide(mu::Range(path, path + strlen(path) + 1));

		HANDLE handle = CreateFile(wide_path.GetRaw(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (handle == INVALID_HANDLE_VALUE)
		{
			return FileReader(nullptr);
		}
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
		if (!reader.IsValidFile()) {
			return {};
		}
		mu::Array<u8> arr;
		size_t size = reader.GetFileSize();
		arr.Reserve(size);
		reader.Read(arr.AddUninitialized(size));
		return arr;
	}

	String LoadFileToString(const char* path) {
		FileReader reader = FileReader::Open(path);
		if (!reader.IsValidFile()) {
			return {};
		}
		if (reader.GetFileSize() < 3) {
			u8 b[3] = { 0, };
			reader.Read(Range(b));
			return String((char*)b);
		}

		char PotentialBOM[3] = { 0, };
		unsigned char BOMSignature[3] = { 0xEF, 0xBB, 0xBF };
		reader.Read(ByteRange(PotentialBOM));
		String s;
		if (memcmp(PotentialBOM, BOMSignature, 3) != 0) {
			// Add those 3 bytes to the string if they aren't a byte order mark
			s = String(Range(PotentialBOM));
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
