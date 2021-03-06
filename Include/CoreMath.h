﻿#pragma once

#include "mu-core/PrimitiveTypes.h"

constexpr f64 Pi = 3.14159265358979323846;
constexpr f64 PiOver2 = 1.57079632679489661923;
constexpr f64 PiOver4 = 0.785398163397448309616;

f32 Cos(f32 radians);
f32 Sin(f32 radians);
f32 Tan(f32 radians);

f64 Cos(f64 radians);
f64 Sin(f64 radians);
f64 Tan(f64 radians);

inline constexpr f64 DegreesToRadians(f64 radians) { return radians *Pi / 180.0; }
inline constexpr f32 DegreesToRadians(f32 radians) { return f32(radians * Pi / 180.0); }

f32 Sqrt(f32 f);
f64 Sqrt(f64 f);
