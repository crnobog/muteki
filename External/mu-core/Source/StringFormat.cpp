#include "mu-core/StringFormat.h"
#include "mu-core/String.h"
#include "mu-core/Debug.h"
#include <stdexcept>

namespace mu {
	StringFormatArg::StringFormatArg(const StringFormatArg& other) : m_type(other.m_type) {
		switch (m_type) {
		case StringFormatArgType::C_Str:
			m_c_str = other.m_c_str;
			return;
		case StringFormatArgType::Unsigned:
			m_uint = other.m_uint;
			return;
		case StringFormatArgType::Double:
			m_double = other.m_double;
			return;
		case StringFormatArgType::Signed:
			m_int = other.m_int;
			return;
		}
		Assert(false);
	}
	StringFormatArg::StringFormatArg(StringFormatArg&& other) : m_type(other.m_type) {
		switch (m_type) {
		case StringFormatArgType::C_Str:
			m_c_str = other.m_c_str;
			return;
		case StringFormatArgType::Unsigned:
			m_uint = other.m_uint;
			return;
		case StringFormatArgType::Double:
			m_double = other.m_double;
			return;
		case StringFormatArgType::Signed:
			m_int = other.m_int;
			return;
		}
		Assert(false);
	}
	StringFormatArg::StringFormatArg(const char* c_str)
		: m_type(StringFormatArgType::C_Str)
		, m_c_str(c_str, strlen(c_str)) {
	}

	StringFormatArg::StringFormatArg(const char* c_str, i64 len)
		: m_type(StringFormatArgType::C_Str)
		, m_c_str(c_str, len) {
	}

	StringFormatArg::StringFormatArg(const String_T<char>& str) {
		if (str.IsEmpty()) {
			m_type = StringFormatArgType::None;
		}
		else {
			m_type = StringFormatArgType::C_Str;
			m_c_str = { str.GetRaw(), str.GetLength() };
		}
	}

	StringFormatArg::StringFormatArg(i32 i)
		: m_type(StringFormatArgType::Signed)
		, m_int(i) {
	}

	StringFormatArg::StringFormatArg(i64 i)
		: m_type(StringFormatArgType::Signed)
		, m_int(i) {
	}

	StringFormatArg::StringFormatArg(u32 u)
		: m_type(StringFormatArgType::Unsigned)
		, m_uint(u) {
	}

	StringFormatArg::StringFormatArg(u64 u)
		: m_type(StringFormatArgType::Unsigned)
		, m_uint(u) {
	}

	StringFormatArg::StringFormatArg(float f)
		: m_type(StringFormatArgType::Double)
		, m_double(f) {
	}

	StringFormatArg::StringFormatArg(double d)
		: m_type(StringFormatArgType::Double)
		, m_double(d) {
	}

	void ValidateFormatString(const char* fmt) {
		ValidateFormatString(fmt, {});
	}

	void ValidateFormatString(const char* fmt, PointerRange<StringFormatArgType> arg_types) {
		// No args overload, the format string should contain no {} blocks

		size_t expected_args = 0;
		for (const char* cursor = fmt; *cursor != '\0'; ++cursor) {
			if (*cursor != '{') { continue; }
			++cursor;
			if (*cursor == '{') {
				// Escaped brace
				continue;
			}
			else if (*cursor == '}') {
				++expected_args;
			}
		}

		if (expected_args != arg_types.Size()) {
			throw std::runtime_error("Format string mismatch");
		}
	}

	void FormatRange(IStringFormatOutput& output, const char* fmt, PointerRange<StringFormatArg> args) {
		const char* block_begin = fmt;
		const char* cursor = fmt;
		for (; *cursor != '\0'; ++cursor) {
			if (*cursor != '{') { continue; }
			if (block_begin != cursor) {
				// Output bare string so far
				output.Write(StringFormatArg{ block_begin, cursor - block_begin });
			}
			++cursor;
			if (*cursor == '{') {
				// Escaped char, begin a new block from here
				block_begin = cursor;
				continue;
			}
			else if (*cursor == '}') {
				// Empty format string specified, emit next unemitted format arg
				output.Write(args.Front());
				args.Advance();
				block_begin = cursor + 1;
				continue; // will increment cursor to == block_begin
			}
			else {
				throw std::runtime_error("Invalid format string, only {} is implemented");
			}
		}

		if (block_begin != cursor) {
			output.Write({ block_begin, cursor - block_begin });
		}
		output.Close();
	}
}
