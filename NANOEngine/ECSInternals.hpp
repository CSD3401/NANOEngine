#pragma once

#include <cstdint>
#include "NANOEngineAPI.hpp"

namespace NANOEngine {
	//extern SceneManagement::Scene scene;

	// --- Entity lifetime ---
	NANOENGINE_API uint32_t   CreateEntity();
	NANOENGINE_API void       DestroyEntity(uint32_t e);

	// --- Component Handling ---
	NANOENGINE_API uint64_t GetEntitySignature(uint32_t e);

}
