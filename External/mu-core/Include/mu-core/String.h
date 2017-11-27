#pragma once

#include "mu-core/Array.h"
#include "mu-core/Ranges.h"
#include "mu-core/StringFormat.h"
#include "mu-core/MetaProgramming.h"

#include <array>


namespace mu {
	// #TODO: Encoding is a template parameter so conversions can be done automatically
	template<typename CharType>
	class String_T {
		Array<CharType> m_data;

	public:
		String_T() = default;
		~String_T() = default;
		String_T(const String_T&) = default;
		String_T(String_T&&) = default;
		String_T& operator=(const String_T&) = default;
		String_T& operator=(String_T&&) = default;

		String_T(const CharType* str) {
			for (; *str != '\0'; ++str) {
				m_data.Add(*str);
			}
			m_data.Add('\0');
		}

		template<typename OtherChar, typename SizeType>
		String_T(std::tuple<OtherChar*, SizeType> t) {
			const auto* str = std::get<0>(t);
			auto len = std::get<1>(t);
			AppendRange(Range(str, str + len));
		}

		template<typename... RANGES>
		explicit String_T(RANGES&&... rs) {
			AppendRanges(Range(std::forward<RANGES>(rs))...);
		}

		bool IsEmpty() const { return m_data.Num() <= 1; }
		const CharType* GetRaw() const { return m_data.Data(); }
		size_t GetLength() const {
			size_t n = m_data.Num();
			return n == 0 ? 0 : n - 1;
		}

		bool operator==(const String_T& other) const {
			for (auto[a, b] : Zip(m_data, other.m_data)) {
				if (a != b) {
					return false;
				}
			}
			return true;
		}

		void Add(CharType c) {
			if (m_data.Num() == 0) {
				m_data.Add(c);
			}
			else {
				m_data[m_data.Num() - 1] = c;
			}
			m_data.Add('\0');
		}
		void Append(const CharType* c) {
			RemoveTrailingNull();
			for (; *c != '\0'; ++c) {
				m_data.Add(*c);
			}
			m_data.Add('\0');
		}
		void Append(const String_T& s) {
			RemoveTrailingNull();
			m_data.Append(s.m_data);
		}

		template<typename RANGE, DisableIf<RANGE::HasSize>...>
		void AppendRange(RANGE r) {
			RemoveTrailingNull();
			for (; !r.IsEmpty(); r.Advance()) {
				m_data.Add(r.Front());
			}
			AddTrailingNull();
		}

		template<typename RANGE, EnableIf<RANGE::HasSize>...>
		void AppendRange(RANGE r) {
			RemoveTrailingNull();
			m_data.Reserve(r.Size());
			for (; !r.IsEmpty(); r.Advance()) {
				m_data.Add(r.Front());
			}
			AddTrailingNull();
		}

		template<typename RANGE, typename... RANGES>
		void AppendRanges(RANGE r, RANGES... rs) {
			AppendRange(r);
			if constexpr(sizeof...(rs) > 0) {
				AppendRanges(rs...);
			}
		}


		int Compare(const char* c_str) const {
			return strncmp(m_data.Data(), c_str, m_data.Num());
		}

		bool operator!=(const String_T& other) {
			if (IsEmpty() != other.IsEmpty()) {
				return false;
			}
			return Compare(other.GetRaw()) != 0;
		}

		bool operator==(const String_T& other) {
			if (IsEmpty() != other.IsEmpty()) {
				return false;
			}
			return Compare(other.GetRaw()) == 0;
		}


		template<typename... ARGS>
		static String_T Format(const char* fmt, ARGS... args) {
			StringFormatOutput output{};
			mu::Format(output, fmt, std::forward<ARGS>(args)...);
			return output.m_string;
		}

	private:

		struct StringFormatOutput : public IStringFormatOutput {
			String_T m_string;
			virtual void Write(StringFormatArg arg) override {
				i32 count = 0;
				char buffer[256];
				switch (arg.m_type) {
				case StringFormatArgType::C_Str:
					m_string.Append(arg.m_c_str);
					break;
				case StringFormatArgType::Unsigned:
					count = snprintf(buffer, 256, "%I64u", arg.m_uint);
					if (count > 0) {
						m_string.Append(buffer);
					}
					break;
				case StringFormatArgType::Double:
					count = snprintf(buffer, 256, "%f", arg.m_double);
					if (count > 0) {
						m_string.Append(buffer);
					}
					break;
				}
			}
			virtual void Close() override {}
		};

		void RemoveTrailingNull() {
			if (m_data.Num() > 0) {
				m_data.RemoveAt(m_data.Num() - 1);
			}
		}
		void AddTrailingNull() {
			if (m_data.Num() > 0 && m_data[m_data.Num() - 1] != 0) {
				m_data.Add(0);
			}
		}
	};


	// UTF-8 string
	using String = String_T<char>;
	// UTF-32 string
	using String32 = String_T<char32_t>;

	template<typename CharType>
	PointerRange<const CharType> Range(const String_T<CharType>& s) {
		return Range(s.GetRaw(), s.GetLength());
	}
	template<typename CharType>
	PointerRange<const CharType> Range(String_T<CharType>& s) {
		return Range(s.GetRaw(), s.GetLength());
	}

	String WideStringToUTF8(PointerRange<const wchar_t> in);
	String_T<wchar_t> UTF8StringToWide(PointerRange<const char> in);
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/String_Tests.inl"
#endif
