#pragma once

#ifdef _MSC_VER
typedef __int8 i8;
typedef __int16 i16;
typedef __int32 i32;
typedef __int64 i64;

typedef unsigned __int8 u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;
typedef unsigned __int64 u64;

typedef float f32;
typedef double f64;

#if defined(_WIN64)
typedef u64 uptr;
typedef i64 iptr;
#else
typedef u32 uptr;
typedef i32 iptr;
#endif

constexpr u16 u16_max = 0xffffu;
constexpr u32 u32_max = 0xffffffffu;
constexpr u64 u64_max = 0xffffffffffffffffull;

#endif