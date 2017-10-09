#pragma once

#include "Global.h"
#include "mu-core/PrimitiveTypes.h"
#include "mu-core/StringFormat.h"
#include "mu-core/PointerRange.h"

namespace mu {
	namespace dbg {
		namespace details {
			enum class LogLevel {
				Log,
				Error
			};
		}

		void LogInternal(details::LogLevel level, const char* fmt, PointerRange<StringFormatArg> args);

		template<typename ...ARGS>
		void Log(const char* fmt, ARGS... args) {
			ValidateFormatString(fmt, args...);

			if constexpr(sizeof...(args) > 0) {
				StringFormatArg wrap_args[] = { StringFormatArg(std::forward<ARGS>(args))... };
				LogInternal(details::LogLevel::Log, fmt, Range(wrap_args));
			}
			else {
				LogInternal(details::LogLevel::Log, fmt, {});
			}
		}

		template<typename ...ARGS>
		void Err(const char* fmt, ARGS... args) {
			ValidateFormatString(fmt, args...);

			if constexpr(sizeof...(args) > 0) {
				StringFormatArg wrap_args[] = { StringFormatArg(std::forward<ARGS>(args))... };
				LogInternal(details::LogLevel::Log, fmt, Range(wrap_args));
			}
			else {
				LogInternal(details::LogLevel::Log, fmt, {});
			}
		}

		template<typename... TS>
		FORCEINLINE bool AssertImpl(bool e, const char* fmt, TS... ts) {
			if (!e) {
				mu::dbg::Err(fmt, std::forward<TS>(ts)...);
				BREAK_OR_CRASH; // TODO: Do not crash if no debugger is attached
				return false;
			}
			return true;
		}
	}
}


#define Assertf(E, FMT, ...) mu::dbg::AssertImpl(E, FMT, __VA_ARGS__)
#define Assert(E) mu::dbg::AssertImpl(E, "Assertion failed: {}", #E)
