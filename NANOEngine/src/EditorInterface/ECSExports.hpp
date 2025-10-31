#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>
#include <typeindex>
#include <vector>
#include "../NANOEngineAPI.hpp"
#include "../Core/Reflection.hpp"

namespace NE::ECS {
	// Forward Decl
	namespace Component {
		struct Transform;
		struct Renderer;
		struct Light;
		struct Rigidbody;
		struct Collider;
		struct EntityMeta;
		struct AudioSource;
		struct NativeScript;
		struct Animator;
		struct PhysicsBody;
	}

	namespace Query {

		NANOENGINE_API std::unordered_map<std::type_index, uint8_t> GetRegisteredComponentTypes();

		// --- Component Handling ---
		NANOENGINE_API uint64_t GetEntitySignature(uint32_t e);

		// --- Editor Component View --- //
		NANOENGINE_API const Component::EntityMeta& GetEntityMeta(uint32_t e);
		NANOENGINE_API const Component::Transform& GetEntityTransform(uint32_t e);
		NANOENGINE_API const Component::Renderer& GetEntityRenderer(uint32_t e);
		NANOENGINE_API const Component::Light& GetEntityLight(uint32_t e);
		NANOENGINE_API const Component::Rigidbody& GetEntityRigidbody(uint32_t e);
		NANOENGINE_API const Component::Collider& GetEntityCollider(uint32_t e);
		NANOENGINE_API const Component::AudioSource& GetEntityAudioSource(uint32_t e);
		NANOENGINE_API const Component::NativeScript& GetEntityScript(uint32_t e);

		NANOENGINE_API bool HasAnimator(uint32_t e);                       // <-- ADD
		NANOENGINE_API const Component::Animator& GetEntityAnimator(uint32_t e);
	}

	namespace Command {
		NANOENGINE_API uint32_t CreateEntity();
		NANOENGINE_API void DestroyEntity(uint32_t e);

		NANOENGINE_API void AddLightComponent(uint32_t e);
		NANOENGINE_API void AddRendererComponent(uint32_t e);
		NANOENGINE_API void AddRigidbodyComponent(uint32_t e);
		NANOENGINE_API void AddColliderComponent(uint32_t e);
		NANOENGINE_API void AddAudioSourceComponent(uint32_t e);
		NANOENGINE_API void AddScriptComponent(uint32_t e);

		// --- Editor Component Mutators --- //
		NANOENGINE_API Component::EntityMeta& GetEntityMeta(uint32_t e);
		NANOENGINE_API Component::Transform& GetEntityTransform(uint32_t e);
		NANOENGINE_API Component::Renderer& GetEntityRenderer(uint32_t e);
		NANOENGINE_API Component::Light& GetEntityLight(uint32_t e);
		NANOENGINE_API Component::Rigidbody& GetEntityRigidbody(uint32_t e);
		NANOENGINE_API Component::Collider& GetEntityCollider(uint32_t e);
		NANOENGINE_API Component::AudioSource& GetEntityAudioSource(uint32_t e);
		NANOENGINE_API Component::NativeScript& GetEntityScript(uint32_t e);

		// --- Script Management ---
		NANOENGINE_API std::vector<std::string> GetRegisteredScriptNames();
		NANOENGINE_API bool SetEntityScript(uint32_t e, const std::string& scriptName);
		NANOENGINE_API void RemoveEntityScript(uint32_t e);
		NANOENGINE_API bool IsScriptRegistered(const std::string& scriptName);

		NANOENGINE_API void AddAnimatorComponent(uint32_t e);              // <-- ADD
		NANOENGINE_API Component::Animator& GetEntityAnimator(uint32_t e);
	}

}
