#pragma once

#include <cstdint>
#include <unordered_map>
#include <string>
#include "NANOEngineAPI.hpp"
#include <typeindex>


namespace NE {
	namespace ECS::Component {
		struct Transform;
		struct Renderer;
		struct Light;
		struct Rigidbody;
		struct Collider;
	}

	NANOENGINE_API void AssignRendererModel(ECS::Component::Renderer& r, std::string filepath);
	NANOENGINE_API void AssignRendererMaterial(ECS::Component::Renderer& r, std::string filepath);

	NANOENGINE_API std::vector<uint32_t>& GetEntities();

	NANOENGINE_API void SetMotionType(uint32_t e, uint8_t motionType);
}
