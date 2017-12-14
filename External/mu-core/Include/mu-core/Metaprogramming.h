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

	template<typename FROM, typename TO>
	struct CopyCVImpl {
	private:
		using C = std::conditional_t< std::is_const_v<FROM>, std::add_const_t<TO>, TO>;
		using V = std::conditional_t< std::is_volatile_v<FROM>, std::add_volatile_t<C>, C>;
	public:
		using Type = V;
	};

	template<typename FROM, typename TO>
	using CopyCV = typename CopyCVImpl<FROM, TO>::Type;
}
