#pragma once

#include <cstdint>
#include <unordered_map>
#include "NANOEngineAPI.hpp"
#include <typeindex>

namespace NANOEngine {
	namespace ECS::Component {
		struct Transform;
	}
	//extern SceneManagement::Scene scene;

	// --- Entity lifetime ---
	NANOENGINE_API uint32_t   CreateEntity();
	NANOENGINE_API void       DestroyEntity(uint32_t e);

	// --- Component Handling ---
	NANOENGINE_API uint64_t GetEntitySignature(uint32_t e);


	NANOENGINE_API ECS::Component::Transform& GetEntityTransform(uint32_t e);

	NANOENGINE_API const std::unordered_map<std::type_index, uint8_t>& GetRegisteredComponentTypes();
}
