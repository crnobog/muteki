#include "GPUInterface.h"

u32 GPU::GetStreamElementSize(const StreamElementDesc& element) {
	switch (element.Type) {
	case ScalarType::Float:
		return element.Count * 4;
	}
	return 0;
}