## FAQ

- [**How is doctest different from Catch?**](#how-is-doctest-different-from-catch)
- [**How to get the best compile-time performance with the framework?**](#how-to-get-the-best-compile-time-performance-with-the-framework)
- [**Is doctest thread-aware?**](#is-doctest-thread-aware)
- [**Is mocking supported?**](#is-mocking-supported)
- [**Why are my tests in a static library not getting registered?**](#why-are-my-tests-in-a-static-library-not-getting-registered)
- [**Why is comparing C strings (```char*```) actually comparing pointers?**](#why-is-comparing-c-strings-char-actually-comparing-pointers)
- [**How to write tests in header-only libraries?**](#how-to-write-tests-in-header-only-libraries)
- [**Does the framework use exceptions?**](#does-the-framework-use-exceptions)
- [**Why do I get compiler errors in STL headers when including the doctest header?**](#why-do-i-get-compiler-errors-in-stl-headers-when-including-the-doctest-header)
- [**Can different versions of the framework be used within the same binary (executable/dll)?**](#can-different-versions-of-the-framework-be-used-within-the-same-binary-executabledll)
- [**Why is doctest using macros?**](#why-is-doctest-using-macros)

### How is **doctest** different from Catch?

Pros of **doctest**:

- including the **doctest** header is [**over 20 times lighter**](benchmarks.md#cost-of-including-the-header) on compile times than that of [**Catch**](https://github.com/philsquared/Catch)
- the asserts in **doctest** can be [**many times lighter**](benchmarks.md#cost-of-an-assertion-macro) on compile times than those of [**Catch**](https://github.com/philsquared/Catch)
- **doctest** executes tests [**many times faster**](benchmarks.md#runtime-benchmarks) than [**Catch**](https://github.com/philsquared/Catch)
- everything testing-related can be removed from the binary by defining the [**```DOCTEST_CONFIG_DISABLE```**](configuration.md#doctest_config_disable) identifier
- doesn't drag any headers when included (except for in the translation unit where the library gets implemented)
- 0 warnings even on the [**most aggressive**](../../scripts/cmake/common.cmake#L84) warning levels for MSVC/GCC/Clang
- per commit tested with 300+ builds on [**much more compilers**](features.md#extremely-portable) - and through valgrind/sanitizers/analyzers
- test cases can be written in headers - the framework will still register the tests only once - no duplicates

Aside from everything mentioned so far doctest has some [**features**](features.md#other-features) (like [**templated test cases**](parameterized-tests.md#templated-test-cases---parameterized-by-type)) which [**Catch**](https://github.com/philsquared/Catch) doesn't.

Missing stuff:

- a reporter/listener system - to a file, to xml, ability for the user to write their own reporter, etc.
- matchers and generators
- other stuff

But these things (and more!) are planned in the [**roadmap**](roadmap.md)!

**doctest** can be thought of as a very polished, light, stable and clean subset (or reimplementation) of [**Catch**](https://github.com/philsquared/Catch) but this might change in the future as more features are added.

A quick and easy way to migrate most of your Catch tests to doctest is to change the ```TEST_CASE``` (if using tags) and ```SECTION``` macros as follows:

```c++
#include "path/to/doctest.h"

#undef TEST_CASE
#define TEST_CASE(name, tags) DOCTEST_TEST_CASE(tags " " name) // will concatenate the tags and test name string literals to one
#define SECTION(name) DOCTEST_SUBCASE(name)
using doctest::Approx; // catch exposes this by default outside of its namespace

```

### How to get the best compile-time performance with the framework?

Using the [**fast**](assertions.md#fast-asserts) asserts in combination with [**```DOCTEST_CONFIG_SUPER_FAST_ASSERTS```**](configuration.md#doctest_config_super_fast_asserts) yelds the [**fastest**](benchmarks.md#cost-of-an-assertion-macro) compile times.

There are only 2 drawbacks of this approach:

- using fast asserts (60-90% [**faster**](benchmarks.md#cost-of-an-assertion-macro) than ```CHECK(a==b)```) means that there is no ```try/catch``` block in each assert so if an expression throws the whole test case ends.
- defining the [**```DOCTEST_CONFIG_SUPER_FAST_ASSERTS```**](configuration.md#doctest_config_super_fast_asserts) config identifier will result in even [**faster**](benchmarks.md#cost-of-an-assertion-macro) fast asserts (50-80%) at the cost of only one thing: when an assert fails and a debugger is present - the framework will break inside a doctest function so the user will have to go 1 level up in the callstack to see where the actual assert is in the source code.

These 2 things can be considered negligible if you are dealing mainly with arithmetic (expressions are unlikely to throw exceptions) and all the tests usually pass (you don't need to often navigate to a failing assert with a debugger attached)

If you want better aliases for the asserts instead of the long ones you could use [**```DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES```**](configuration.md#doctest_config_no_short_macro_names) and then define your aliases like this: ```#define CHECK_EQ DOCTEST_FAST_CHECK_EQ``` (example in [**here**](../../examples/all_features/alternative_macros.cpp)).

### Is doctest thread-aware?

Currently no. Asserts cannot be used in multiple threads and test cases cannot be ran in parallel. These are long-term features that are planned on the [**roadmap**](roadmap.md).

For now tests are ran serially and doing asserts in multiple user threads will lead to crashes.

There is an option to run a [**range**](commandline.md) of tests from an executable - so  ran in parallel with multiple process invocations - see [**the example python script**](../../examples/range_based_execution.py).

### Is mocking supported?

**doctest** doesn't support mocking but should be easy to integrate with third-party libraries such as:

- [trompeloeil](https://github.com/rollbear/trompeloeil) - integration shown [here](https://github.com/rollbear/trompeloeil/blob/master/docs/CookBook.md#adapt_doctest)
- [googlemock](https://github.com/google/googletest/tree/master/googlemock) - for integration check [this](https://github.com/google/googletest/blob/master/googlemock/docs/ForDummies.md#using-google-mock-with-any-testing-framework)
- [FakeIt](https://github.com/eranpeer/FakeIt) - integration might be similar to that of [catch](https://github.com/eranpeer/FakeIt/tree/master/config/catch) but this has not been looked into

by using the [**logging**](logging.md#messages-which-can-optionally-fail-test-cases) macros such as ```ADD_FAIL_AT(file, line, message)```

<!--
Not sure how to integrate with these:
https://github.com/dascandy/hippomocks
https://github.com/tpounds/mockitopp
-->

### Why are my tests in a static library not getting registered?

This is a [**common problem among libraries with self-registering code**](https://groups.google.com/forum/#!msg/catch-forum/FV0Qo62DvgY/jxEO6c9_q3kJ) and it affects all modern compilers on all platforms.

The problem is that when a static library is being linked to a binary (executable or dll) - only object files from the static library that define a symbol being required from the binary will get pulled in (this is a linker/dependency optimization).

A way to solve this in CMake is to use object libraries instead of static libraries - like this:

```cmake
add_library(with_tests OBJECT src_1.cpp src_2.cpp src_3.cpp ...)

add_library(dll SHARED $<TARGET_OBJECTS:with_tests> dll_src_1.cpp ...)
add_executable(exe $<TARGET_OBJECTS:with_tests> exe_src_1.cpp ...)
```

Thanks to [pthom](https://github.com/pthom) for suggesting this.

As an alternative I have created a CMake function that forces every object file from a static library to be linked into a binary target - it is called [**```doctest_force_link_static_lib_in_target()```**](../../examples/exe_with_static_libs/doctest_force_link_static_lib_in_target.cmake). It is unintrusive - no source file gets changed - everything is done with compiler flags per source files. An example project using it can be found [**here**](../../examples/exe_with_static_libs) - the commented part of the CMakeLists.txt file.

It doesn't work in 2 scenarios:

- either the target or the library uses a precompiled header - see [**this**](https://github.com/onqtam/doctest/issues/21#issuecomment-247001423) issue for details
- either the target or the library is an imported target (pre-built) and not built within the current cmake tree

You can also checkout this repository for a different solution: [**pthom/doctest_registerlibrary**](https://github.com/pthom/doctest_registerlibrary).

### Why is comparing C strings (```char*```) actually comparing pointers?

**doctest** by default treats ```char*``` as normal pointers. Using the [**```DOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING```**](configuration.md#doctest_config_treat_char_star_as_string) changes that.

### How to write tests in header-only libraries?

There are 2 options:

- just include the doctest header in your headers and write the tests - the doctest header should be shipped with your headers and the user will have to implement the doctest runner in one of his source files.
- don't include the doctest header and guard your test cases with ```#ifdef DOCTEST_LIBRARY_INCLUDED``` and ```#endif``` - that way your tests will be compiled and registered if the user includes the doctest header before your headers (and he will also have to implement the test runner somewhere).

Also note that it would be a good idea to add a tag in your test case names (like this: ```TEST_CASE("[the_lib] testing foo")```) so the user can easily filter them out with ```--test-case-exclude=*the_lib*``` if he wishes to.

### Does the framework use exceptions?

Yes - but they can be disabled - see the [**```DOCTEST_CONFIG_NO_EXCEPTIONS```**](configuration.md#doctest_config_no_exceptions) config identifier.

### Why do I get compiler errors in STL headers when including the doctest header?

Try using the [**```DOCTEST_CONFIG_USE_IOSFWD```**](configuration.md#doctest_config_use_iosfwd) configuration identifier.

### Can different versions of the framework be used within the same binary (executable/dll)?

Currently no. Single header libraries like [**stb**](https://github.com/nothings/stb) have this as an option (everything gets declared static - making it with internal linkage) but it isn't very logical for **doctest** - the main point is to write tests in any source file of the project and have the test runner implemented in only one source file.

### Why is doctest using macros?

Aren't they evil and not *modern*? - Check out the answer Phil Nash gives to this question [**here**](http://accu.org/index.php/journals/2064) (the creator of [**Catch**](https://github.com/philsquared/Catch)).

---------------

[Home](readme.md#reference)
