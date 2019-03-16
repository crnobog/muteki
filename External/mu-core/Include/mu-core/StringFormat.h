#pragma once

#include "mu-core/Global.h"
#include "mu-core/PrimitiveTypes.h"
#include "mu-core/Metaprogramming.h"
#include "mu-core/PointerRange.h"
#include <tuple>
#include <utility>

enum class StringFormatArgType {
	None,
	C_Str,
	Unsigned,
	Double,
};

namespace mu {
	template<typename T> class String_T;

	struct StringFormatArg {
		StringFormatArgType m_type = StringFormatArgType::None;
		union {
			std::tuple<const char*, i64> m_c_str;
			u64 m_uint;
			double m_double;
		};
		StringFormatArg() {}
		StringFormatArg(const StringFormatArg& other);
		StringFormatArg(StringFormatArg&& other);

		// Single argument, can be inferred from a single template parameter
		StringFormatArg(const char* c_str);
		StringFormatArg(const String_T<char>& str);
		StringFormatArg(i32 i);
		StringFormatArg(u32 u);
		StringFormatArg(float f);
		StringFormatArg(double d);
		StringFormatArg(size_t s);

		template<typename RANGE, EnableIf<RANGE::IsContiguous && RANGE::HasSize>...>
		StringFormatArg(RANGE r)
			: m_c_str(&r.Front(), r.Size())
			, m_type(StringFormatArgType::C_Str) {}

		// Can we use this with std::initializer list as en entry in a parameter pack?
		StringFormatArg(const char* c_str, i64 len);
	};

	struct IStringFormatOutput {
		virtual void Write(StringFormatArg arg) = 0;
		virtual void Close() = 0;
	};

	void FormatRange(IStringFormatOutput&, const char* fmt, PointerRange<StringFormatArg> args);

	template<typename... ARGS>
	void Format(IStringFormatOutput& output, const char* fmt, ARGS... args) {
		ValidateFormatString(fmt, args...);
		StringFormatArg wrapped_args[] = { StringFormatArg(args)... };
		FormatRange(output, fmt, Range(wrapped_args));
	}

	template<typename T>
	StringFormatArgType GetStringFormatArgType(T&& t) { return StringFormatArg(t).m_type; }

	void ValidateFormatString(const char* fmt);
	void ValidateFormatString(const char* fmt, PointerRange<StringFormatArgType> arg_types);

	template<typename... ARGS>
	void ValidateFormatString(const char* fmt, ARGS... args) {
		StringFormatArgType arg_types[] = { StringFormatArg(args).m_type... };
		ValidateFormatString(fmt, Range(arg_types));
	}
}
