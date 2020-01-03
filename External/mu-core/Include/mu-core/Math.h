#pragma once

#include "PrimitiveTypes.h"

template<typename A, typename B, typename C>
A Clamp(A a, B b, C c) {
	if (a > c) { return c; }
	if (a < b) { return b; }
	return a;
}

template<typename T>
T Min(const T& a, const T& b) { return a < b ? a : b; }
template<typename T>
T Max(const T& a, const T& b) { return a > b ? a : b; }

template<typename T>
T Abs(T t) { return t >(T)0 ? t : -t; }

template<typename T> 
inline constexpr T AlignPow2(T t, T align) {
	align -= 1;
	return (T)((t + align) & ~align);
}

inline bool IsPowerOf2(i16 i) { return i && !(i & (i - 1)); }
inline bool IsPowerOf2(i32 i) { return i && !(i & (i - 1)); }
inline bool IsPowerOf2(i64 i) { return i && !(i & (i - 1)); }
inline bool IsPowerOf2(u16 i) { return i && !(i & (i - 1)); }
inline bool IsPowerOf2(u32 i) { return i && !(i & (i - 1)); }
inline bool IsPowerOf2(u64 i) { return i && !(i & (i - 1)); }

template<typename T>
inline T AlignUp(T t, T alignment) {
	if (IsPowerOf2(alignment)) {
		return AlignPow2(t, alignment);
	}

	return (((t-1) / alignment) + 1)* alignment;
}


#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/Math_Tests.inl"
#endif