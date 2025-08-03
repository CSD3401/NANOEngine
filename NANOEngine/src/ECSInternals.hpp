#pragma once

#include <cstdint>
#include <unordered_map>
#include <string>
#include "NANOEngineAPI.hpp"
#include <typeindex>


namespace NANOEngine {
	namespace ECS::Component {
		struct Transform;
		struct Renderer;
		struct Light;
		struct Rigidbody;
		struct Collider;
	}
	//extern SceneManagement::Scene scene;

	// --- Entity lifetime ---
	NANOENGINE_API uint32_t   CreateEntity();
	NANOENGINE_API void       DestroyEntity(uint32_t e);

	// --- Component Handling ---
	NANOENGINE_API uint64_t GetEntitySignature(uint32_t e);


	NANOENGINE_API ECS::Component::Transform& GetEntityTransform(uint32_t e);
	NANOENGINE_API ECS::Component::Renderer& GetEntityRenderer(uint32_t e);
	NANOENGINE_API ECS::Component::Light& GetEntityLight(uint32_t e);
	NANOENGINE_API ECS::Component::Rigidbody& GetEntityRigidbody(uint32_t e);
	NANOENGINE_API ECS::Component::Collider& GetEntityCollider(uint32_t e);

	NANOENGINE_API void AddLightComponent(uint32_t e);
	NANOENGINE_API void AddRendererComponent(uint32_t e);
	NANOENGINE_API void AddRigidbodyComponent(uint32_t e);
	NANOENGINE_API void AddColliderComponent(uint32_t e);

	NANOENGINE_API void AssignRendererModel(ECS::Component::Renderer& r, std::string filepath);
	NANOENGINE_API void AssignRendererMaterial(ECS::Component::Renderer& r, std::string filepath);

	NANOENGINE_API const std::unordered_map<std::type_index, uint8_t>& GetRegisteredComponentTypes();

	NANOENGINE_API std::vector<uint32_t>& GetEntities();

	NANOENGINE_API void SetMotionType(uint32_t e, uint8_t motionType);
}
