#pragma once

#include <type_traits>
#include <utility>

namespace mu
{	
	struct TrueType {
		static constexpr bool Value = true;
	};
	struct FalseType {
		static constexpr bool Value = false;
	};
}

#define DEFINE_HAS_STATIC_MEMBER(MEMBER_NAME) \
	template<typename T> class Has_##MEMBER_NAME {\
		template<typename U> static auto Test(int) -> decltype(std::decay_t<T>:: MEMBER_NAME, mu::TrueType{}); \
		template<typename> static auto Test(...) -> mu::FalseType; \
	public: \
		static constexpr bool Value = decltype(Test<T>(0))::Value; \
	};

#define DEFINE_DETECTOR(STRUCT_NAME, EXPRESSION) \
		template<class T, class = std::void_t<> > struct STRUCT_NAME : mu::FalseType {}; \
		template<class T> struct STRUCT_NAME<T, std::void_t< \
			decltype( EXPRESSION ) \
		>> : mu::TrueType {};

#define DEFINE_HAS_INSTANCE_MEMBER(MEMBER_NAME) DEFINE_DETECTOR(Has_##MEMBER_NAME, std::declval<T>().MEMBER_NAME() )