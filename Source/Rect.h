#pragma once

#include "mu-core/PrimitiveTypes.h"

template<typename T>
struct Rect {
	T Left, Top, Right, Bottom;
};

using Rectf = Rect<f32>;
using Recti = Rect<i32>;