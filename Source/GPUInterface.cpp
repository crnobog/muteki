#include "GPUInterface.h"

u32 GPU::GetStreamElementSize(const StreamElementDesc& element) {
	switch (element.Type) {
	case ScalarType::Float:
		return (element.CountMinusOne+1) * 4;
	case ScalarType::U8:
		return (element.CountMinusOne + 1);
	}
	Assert(false);
	return 0;
}