#pragma once

#define STRING_JOIN2(arg1, arg2) DO_STRING_JOIN2(arg1, arg2)
#define DO_STRING_JOIN2(arg1, arg2) arg1 ## arg2
#define SCOPE_EXIT(BLOCK) auto STRING_JOIN2(scope_exit_, __LINE__) = mu::make_scope_exit([&](){ BLOCK ;});

#define FORCEINLINE __forceinline
#define FORCENOINLINE __declspec(noinline)
#define BREAK_OR_CRASH (__debugbreak(),(*(int*)nullptr)++)

template<typename T>
struct ShowTypeHelper;

#define SHOW_TYPE(VAR) ShowTypeHelper<decltype(VAR)> STRING_JOIN2(showtype_helper, __LINE__);
