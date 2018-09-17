#pragma once

#include "PrimitiveTypes.h"

class Timer {
	i64 m_start = 0, m_end = 0;

public:
	Timer();

	void Reset();
	void Stop();
	double GetElapsedTimeSeconds() const;
	double GetElapsedTimeMilliseconds() const;
};
