# Benchmarks

The benchmarks are done with [**this**](../../scripts/bench/bench.py) script using CMake. There are 3 benchmarking scenarios:

- [the cost of including the header](#cost-of-including-the-header)
- [the cost of an assertion macro](#cost-of-an-assertion-macro)
- [runtime speed of lots of asserts](#runtime-benchmarks)

Compilers used:

- WINDOWS: Microsoft Visual Studio Community 2017 - Version 15.1.26403.7
- WINDOWS: gcc 7.1.0 (x86_64-posix-seh-rev0, Built by MinGW-W64 project)
- LINUX: gcc 6.3.0 20170406 (Ubuntu 6.3.0-12ubuntu2)
- LINUX: clang 4.0.0-1 (tags/RELEASE_400/rc1) Target: x86_64-pc-linux-gnu

Environment used (Intel i7 3770k, 16g RAM):

- Windows 7 - on an SSD
- Ubuntu 17.04 in a VirtualBox VM - on a HDD

**doctest** version: 1.2.0 (released on 2017.05.16)

[**Catch**](https://github.com/philsquared/Catch) version: 1.9.3 (released on 2017.04.25)

# Compile time benchmarks

## Cost of including the header

This is a benchmark that is relevant only to single header and header only frameworks - like **doctest** and [**Catch**](https://github.com/philsquared/Catch).

The script generates 201 source files and in 200 of them makes a function in the form of ```int f135() { return 135; }``` and in ```main.cpp``` it forward declares all the 200 such dummy functions and accumulates their result to return from the ```main()``` function. This is done to ensure that all source files are built and that the linker doesn't remove/optimize anything.

- **baseline** - how much time the source files need for a single threaded build with ```msbuild```/```make```
- **+ implement** - only in ```main.cpp``` the header is included with a ```#define``` before it so the test runner gets implemented:

```c++
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```
- **+ header everywhere** - the framework header is also included in all the other source files
- **+ disabled** - **doctest** specific - only this framework can remove everything related to it from the binary

| doctest             | baseline | + implement | + header everywhere | + disabled |
|---------------------|----------|-------------|---------------------|------------|
| MSVC Debug          |    7.17 |    8.50 |   12.50 |    9.42 |
| MSVC Release        |    6.72 |    8.47 |   12.51 |    8.97 |
| MinGW GCC Debug     |   10.65 |   13.56 |   18.59 |   13.34 |
| MinGW GCC Release   |   10.81 |   14.53 |   19.21 |   14.11 |
| Linux GCC Debug     |    4.98 |    6.55 |   11.22 |    6.94 |
| Linux GCC Release   |    4.49 |    7.27 |   11.92 |    7.87 |
| Linux Clang Debug   |    8.52 |    9.42 |   15.25 |   11.14 |
| Linux Clang Release |    9.01 |   11.85 |   18.18 |   11.75 |

| Catch               | baseline | + implement | + header everywhere |
|---------------------|----------|-------------|---------------------|
| MSVC Debug          |    7.17 |   10.60 |  128.02 |
| MSVC Release        |    6.72 |   10.90 |  119.25 |
| MinGW GCC Debug     |   10.65 |   26.12 |  127.00 |
| MinGW GCC Release   |   10.81 |   19.95 |  114.15 |
| Linux GCC Debug     |    4.98 |    9.37 |  105.66 |
| Linux GCC Release   |    4.49 |   13.04 |  105.57 |
| Linux Clang Debug   |    8.52 |   11.33 |   70.57 |
| Linux Clang Release |    9.01 |   16.59 |   75.85 |

<img src="../../scripts/data/benchmarks/header.png" width="430" align="right">
<img src="../../scripts/data/benchmarks/implement.png" width="430">

### Conclusion

#### doctest

- instantiating the test runner in one source file costs ~1.5-3 seconds ```implement - baseline```
- the inclusion of ```doctest.h``` in one source file costs between 20ms - 30ms ```(header_everywhere - implement) / 200```
- including the library everywhere but everything disabled costs less than 3 seconds ```disabled - baseline``` for 200 files

#### [Catch](https://github.com/philsquared/Catch)

- instantiating the test runner in one source file costs ~4-8 seconds ```implement - baseline```
- the inclusion of ```catch.hpp```  in one source file costs between 300ms - 575ms ```(header_everywhere - implement) / 200```

----------

So if ```doctest.h``` costs 20ms and ```catch.hpp``` costs 560ms on MSVC - then the **doctest** header is >> **28** << times lighter (for MSVC)!

----------

The results are in seconds and are in **no way** intended to bash [**Catch**](https://github.com/philsquared/Catch) - the **doctest** framework wouldn't exist without it.

The reason the **doctest** header is so light on compile times is because it forward declares everything and doesn't drag any headers in the source files (except for the source file where the test runner gets implemented). This was a key design decision.

## Cost of an assertion macro

The script generates 11 ```.cpp``` files and in 10 of them makes 50 test cases with 100 asserts in them (of the form ```CHECK(a==b)``` where ```a``` and ```b``` are always the same ```int``` variables) - **50k** asserts! The testing framework gets implemented in ```main.cpp```.

- **baseline** - how much time a single threaded build takes with the header included everywhere - no test cases or asserts!
- ```CHECK(a==b)``` - will add ```CHECK()``` asserts which decompose the expression with template machinery

**doctest** specific:

- ```CHECK_EQ(a,b)``` - will use ```CHECK_EQ(a,b)``` instead of the expression decomposing ones
- ```FAST_CHECK_EQ(a,b)``` - will use ```FAST_CHECK_EQ(a,b)``` instead of the expression decomposing ones
- **+faster** - will add [**```DOCTEST_CONFIG_SUPER_FAST_ASSERTS```**](configuration.md#doctest_config_super_fast_asserts) which speeds up ```FAST_CHECK_EQ(a,b)``` even more
- **+disabled** - all test case and assert macros will be disabled with [**```DOCTEST_CONFIG_DISABLE```**](configuration.md#doctest_config_disable)

[**Catch**](https://github.com/philsquared/Catch) specific:

- **+faster** - will add [**```CATCH_CONFIG_FAST_COMPILE```**](https://github.com/philsquared/Catch/blob/master/docs/configuration.md#catch_config_fast_compile) which speeds up the compilation of the normal asserts ```CHECK(a==b)```

| doctest             | baseline | ```CHECK(a==b)``` | ```CHECK_EQ(a,b)``` | ```FAST_CHECK_EQ(a,b)``` | +faster | +disabled |
|---------------------|----------|-------------------|---------------------|--------------------------|---------|-----------|
| MSVC Debug          |    3.22 |   24.17 |   18.54 |    8.32 |    5.75 |    2.34 |
| MSVC Release        |    3.63 |   41.10 |   23.20 |   10.89 |    6.87 |    2.31 |
| MinGW GCC Debug     |    4.09 |   91.98 |   60.19 |   25.51 |   12.61 |    1.82 |
| MinGW GCC Release   |    4.74 |  240.58 |  156.79 |   50.16 |   19.72 |    2.53 |
| Linux GCC Debug     |    2.06 |   81.32 |   52.14 |   18.07 |   10.15 |    1.16 |
| Linux GCC Release   |    3.28 |  207.21 |  126.89 |   33.17 |   19.92 |    2.03 |
| Linux Clang Debug   |    1.75 |   79.41 |   51.20 |   17.78 |    7.65 |    1.20 |
| Linux Clang Release |    3.73 |  140.79 |   82.61 |   21.19 |   12.64 |    1.46 |

And here is [**Catch**](https://github.com/philsquared/Catch) which only has normal ```CHECK(a==b)``` asserts:

| Catch               | baseline | ```CHECK(a==b)``` | +faster |
|---------------------|----------|-------------------|---------|
| MSVC Debug          |    9.94 |   40.14 |   36.66 |
| MSVC Release        |   10.66 |  231.60 |   81.90 |
| MinGW GCC Debug     |   21.20 |  129.26 |  110.95 |
| MinGW GCC Release   |   14.59 |  297.04 |  207.75 |
| Linux GCC Debug     |   10.05 |  115.53 |   98.84 |
| Linux GCC Release   |   13.29 |  294.26 |  218.37 |
| Linux Clang Debug   |    6.38 |  103.06 |   85.02 |
| Linux Clang Release |   11.15 |  195.62 |  156.04 |

<img src="../../scripts/data/benchmarks/asserts.png">

### Conclusion

**doctest**:

- is around 30% faster than [**Catch**](https://github.com/philsquared/Catch) when using normal expression decomposing ```CHECK(a==b)``` asserts
- asserts of the form ```CHECK_EQ(a,b)``` with no expression decomposition - around 25%-45% faster than ```CHECK(a==b)```
- fast asserts like ```FAST_CHECK_EQ(a,b)``` with no ```try/catch``` blocks - around 60-80% faster than ```CHECK_EQ(a,b)```
- the [**```DOCTEST_CONFIG_SUPER_FAST_ASSERTS```**](configuration.md#doctest_config_super_fast_asserts) identifier which makes the fast assertions even faster by another 50-80%
- using the [**```DOCTEST_CONFIG_DISABLE```**](configuration.md#doctest_config_disable) identifier the assertions just disappear as if they were never written

[**Catch**](https://github.com/philsquared/Catch):

- using [**```CATCH_CONFIG_FAST_COMPILE```**](https://github.com/philsquared/Catch/blob/master/docs/configuration.md#catch_config_fast_compile) results in 10%-40% faster build times for asserts.

## Runtime benchmarks

The runtime benchmarks consist of a single test case with a loop of 10 million iterations performing the task - a single normal assert (using expression decomposition) or the assert + the logging of the loop iterator ```i```:

```c++
for(int i = 0; i < 10000000; ++i)
    CHECK(i == i);
```

or

```c++
for(int i = 0; i < 10000000; ++i) {
    INFO(i);
    CHECK(i == i);
}
```

Note that the assert always passes - the goal should be to optimize for the common case - lots of passing test cases and a few that maybe fail.

| doctest             | assert  | + info  | &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; | Catch               | assert  | + info  |
|---------------------|---------|---------|-|---------------------|---------|---------|
| MSVC Debug          |    5.29 |   14.32 | | MSVC Debug          |  365.37 |  621.78 |
| MSVC Release        |    0.77 |    1.62 | | MSVC Release        |    7.04 |   18.64 |
| MinGW GCC Debug     |    2.25 |    4.71 | | MinGW GCC Debug     |    9.22 |   21.89 |
| MinGW GCC Release   |    0.39 |    0.90 | | MinGW GCC Release   |    7.29 |   13.95 |
| Linux GCC Debug     |    2.84 |    5.13 | | Linux GCC Debug     |   11.17 |   24.79 |
| Linux GCC Release   |    0.30 |    0.69 | | Linux GCC Release   |    6.45 |   12.68 |
| Linux Clang Debug   |    2.47 |    5.02 | | Linux Clang Debug   |   10.40 |   22.64 |
| Linux Clang Release |    0.41 |    0.75 | | Linux Clang Release |    5.81 |   13.83 |

<img src="../../scripts/data/benchmarks/runtime_info.png" width="430" align="right">
<img src="../../scripts/data/benchmarks/runtime_assert.png" width="430">

Note that in these graphs the values for ```MSVC Release``` for **Catch** are 10 times smaller than the real ones (from the tables above) because google spreadsheet didn't allow me to create a bar chart with values that were so different.

### Conclusion

**doctest** is significantly faster - between 4 and 40 times.

In these particular cases **doctest** makes 0 allocations when the assert doesn't fail - it uses lazy stringification (meaning it stringifies the expression or the logged loop counter only if it has to) and a small-buffer optimized string class to achieve these results.

----------

If you want a benchmark that is not synthetic - check out [**this blog post**](http://baptiste-wicht.com/posts/2016/09/blazing-fast-unit-test-compilation-with-doctest-11.html) of [**Baptiste Wicht**](https://github.com/wichtounet) who tested the compile times of the asserts in the 1.1 release with his [**Expression Templates Library**](https://github.com/wichtounet/etl)!

While reading the post - keep in mind that if a part of a process takes 50% of the time and is made 10000 times faster - the overall process would still be only roughly 50% faster.

---------------

[Home](readme.md#reference)
