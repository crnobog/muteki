#pragma once

#include "mu-core/Ranges.h"
#include "mu-core/Algorithms.h"

namespace mu {
	namespace paths {
		PointerRange<const char> GetExecutablePath();
		PointerRange<const char> GetExecutableDirectory();

		PointerRange<const char> GetDirectory(PointerRange<const char> path);
		PointerRange<const char> GetFilename(PointerRange<const char> path);
		PointerRange<const char> GetExtension(PointerRange<const char> path);
	}
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/Paths_Tests.inl"
#endif