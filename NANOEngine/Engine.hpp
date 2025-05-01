#pragma once

#include "NANOEngineAPI.hpp"

namespace NANOEngine {
	NANOENGINE_API void Initialize();
	NANOENGINE_API void Shutdown();
	NANOENGINE_API void* GetNativeWindowHandle();
}
