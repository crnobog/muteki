#include "GPU_Common.h"

namespace GPUCommon {
	u32 GetStreamElementSize(const StreamElementDesc& element) {
		switch (element.Type) {
		case ScalarType::Float:
			return element.Count * 4;
		}
		return 0;
	}
}