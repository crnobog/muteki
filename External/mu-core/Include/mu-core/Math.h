#pragma once

template<typename A, typename B, typename C>
A Clamp(A a, B b, C c)
{
	if (a > c) { return c; }
	if (a < b) { return b; }
	return a;
}

template<typename T>
T Min(const T& a, const T& b) { return a < b ? a : b; }
template<typename T>
T Max(const T& a, const T& b) { return a > b ? a : b; }