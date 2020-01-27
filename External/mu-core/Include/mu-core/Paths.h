#pragma once

#include <filesystem>

namespace mu {
	namespace paths {
		std::filesystem::path GetExecutablePath();
		std::filesystem::path GetExecutableDirectory();
	}
}

#ifdef DOCTEST_LIBRARY_INCLUDED
#include "Tests/Paths_Tests.inl"
#endif