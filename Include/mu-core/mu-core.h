#pragma once

#ifdef MU_CORE_IMPL

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

#endif

#include "PrimitiveTypes.h"
#include "AdaptiveRadixTree.h"
#include "Algorithms.h"
#include "Array.h"
#include "BitArray.h"
#include "Debug.h"
#include "FileReader.h"
#include "FixedArray.h"
#include "Functors.h"
#include "HashTable.h"
#include "Math.h"
#include "Metaprogramming.h"
#include "Paths.h"
#include "Pool.h"
#include "Ranges.h"
#include "RangeWrappers.h"
#include "RefCountPtr.h"
#include "Scope.h"
#include "String.h"
#include "Utils.h"
