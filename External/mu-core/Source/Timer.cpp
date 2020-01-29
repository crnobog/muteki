#include "mu-core/Timer.h"

#if defined(_WIN32) || defined(_WIN64)

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

// TODO: Rewrite to use std::chrono?
namespace mu {
	static const f64 g_cycles_per_second = []() -> f64 {
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		return (f64)freq.QuadPart;
	}();

	Timer::Timer() {
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);
		m_start = time.QuadPart;
	}

	void Timer::Reset() {
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);
		m_start = time.QuadPart;
		m_end = 0;
	}

	void Timer::Stop() {
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);
		m_end = time.QuadPart;
	}

	double Timer::GetElapsedTimeSeconds() const {
		if (m_end > 0) {
			return (m_end - m_start) / (double)g_cycles_per_second;
		}
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);
		return (time.QuadPart - m_start) / (double)g_cycles_per_second;
	}

	double Timer::GetElapsedTimeMilliseconds() const {
		if (m_end > 0) {
			return (m_end - m_start) / (double)g_cycles_per_second * 1000.0;
		}
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);
		return (time.QuadPart - m_start) / (double)g_cycles_per_second * 1000.0;
	}
}
#endif
