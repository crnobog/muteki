#pragma once

#include <type_traits>

// Debugging helper. Specialize this template and read the compiler error to see a typename.
template<typename T>
struct TD;

namespace mu {
	namespace enable_if_details {
		enum class enabler {};
		enum class disabler {};
	}

	template<bool Condition>
	using EnableIf = typename std::enable_if<Condition, enable_if_details::enabler>::type;

	template<bool Condition>
	using DisableIf = typename std::enable_if<!Condition, enable_if_details::disabler>::type;
}