#pragma once

#include <cstdint>
#include <unordered_map>
#include <string>
#include <typeindex>
#include "../NANOEngineAPI.hpp"

namespace NANOEngine::ECS::Component {
	struct Transform;
	struct Renderer;
	struct Light;
	struct Rigidbody;
	struct Collider;
}

namespace NE::ECS {

	namespace Query {
		NANOENGINE_API const NANOEngine::ECS::Component::Transform& GetEntityTransform(uint32_t e);
		NANOENGINE_API const NANOEngine::ECS::Component::Renderer& GetEntityRenderer(uint32_t e);
		//NANOENGINE_API const NANOEngine::ECS::Component::Light& GetEntityLight(uint32_t e);
		NANOENGINE_API const NANOEngine::ECS::Component::Rigidbody& GetEntityRigidbody(uint32_t e);
		//NANOENGINE_API const NANOEngine::ECS::Component::Collider& GetEntityCollider(uint32_t e);
	}

	namespace Command {

	}

}
