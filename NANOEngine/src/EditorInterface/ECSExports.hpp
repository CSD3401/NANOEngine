#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>
#include <typeindex>
#include <vector>
#include "../NANOEngineAPI.hpp"
#include "../Core/Reflection.hpp"
#include "ECS/Core/Entity.hpp"
#include "SceneManagement/SceneManager.hpp"
#include "Engine.hpp"
#include "Core/Layers.hpp"

//namespace NE {
//	SceneManagement::Scene& GetScene();
//}

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
		struct Camera;
		struct PhysicsBody;
		struct UIRectTransform;
		struct UIImage;
		struct UICanvas;
		struct Hierarchy;
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
		NANOENGINE_API const Component::UIRectTransform& GetUIRectTransform(uint32_t e);
		NANOENGINE_API const Component::UIImage& GetUIImage(uint32_t e);
		NANOENGINE_API const Component::UICanvas& GetUICanvas(uint32_t e);
		NANOENGINE_API const Component::Hierarchy& GetEntityHierarchy(uint32_t e);
		NANOENGINE_API const Component::Animator& GetEntityAnimator(uint32_t e);
		NANOENGINE_API const Component::Camera& GetEntityCamera(uint32_t e);

		// to move to this in the future
		// also need to find a way to enforce C as component
		template <typename C>
		const C& GetComponent(Entity e);

		template<> inline const Component::EntityMeta& GetComponent<Component::EntityMeta>(uint32_t e) { return GetEntityMeta(e); }
		template<> inline const Component::Transform& GetComponent<Component::Transform>(uint32_t e) { return GetEntityTransform(e); }
		template<> inline const Component::Renderer& GetComponent<Component::Renderer>(uint32_t e) { return GetEntityRenderer(e); }
		template<> inline const Component::Light& GetComponent<Component::Light>(uint32_t e) { return GetEntityLight(e); }
		template<> inline const Component::Rigidbody& GetComponent<Component::Rigidbody>(uint32_t e) { return GetEntityRigidbody(e); }
		template<> inline const Component::Collider& GetComponent<Component::Collider>(uint32_t e) { return GetEntityCollider(e); }
		template<> inline const Component::NativeScript& GetComponent<Component::NativeScript>(uint32_t e) { return GetEntityScript(e); }
		template<> inline const Component::UIRectTransform& GetComponent<Component::UIRectTransform>(uint32_t e) { return GetUIRectTransform(e); }
		template<> inline const Component::UIImage& GetComponent<Component::UIImage>(uint32_t e) { return GetUIImage(e); }
		template<> inline const Component::UICanvas& GetComponent<Component::UICanvas>(uint32_t e) { return GetUICanvas(e); }
		template<> inline const Component::Hierarchy& GetComponent<Component::Hierarchy>(uint32_t e) { return GetEntityHierarchy(e); }
		template<> inline const Component::Camera& GetComponent<Component::Camera>(uint32_t e) { return GetEntityCamera(e); }
		template<> inline const Component::Animator& GetComponent<Component::Animator>(uint32_t e) { return GetEntityAnimator(e); }


		NANOENGINE_API bool HasEntityMeta(uint32_t e);
		NANOENGINE_API bool HasHierarchy(uint32_t e);
		NANOENGINE_API bool HasTransform(uint32_t e);
		NANOENGINE_API bool HasUIRectTransform(uint32_t e);
		NANOENGINE_API bool HasUICanvas(uint32_t e);
		NANOENGINE_API bool HasUIImage(uint32_t e);

		// --- Component Existence Checks ---
		NANOENGINE_API bool HasTransform(uint32_t e);
		NANOENGINE_API bool HasRenderer(uint32_t e);
		NANOENGINE_API bool HasLight(uint32_t e);
		NANOENGINE_API bool HasRigidbody(uint32_t e);
		NANOENGINE_API bool HasCollider(uint32_t e);
		NANOENGINE_API bool HasAudioSource(uint32_t e);
		NANOENGINE_API bool HasScript(uint32_t e);
		NANOENGINE_API bool HasAnimator(uint32_t e);
		NANOENGINE_API bool HasCamera(uint32_t e);

		template <typename C>
		bool HasComponent(Entity e);

		template<> inline bool HasComponent<Component::EntityMeta>(uint32_t e) { return HasEntityMeta(e); }
		template<> inline bool HasComponent<Component::Hierarchy>(uint32_t e) { return HasHierarchy(e); }
		template<> inline bool HasComponent<Component::Transform>(uint32_t e) { return HasTransform(e); }
		template<> inline bool HasComponent<Component::UIRectTransform>(uint32_t e) { return HasUIRectTransform(e); }
		template<> inline bool HasComponent<Component::UICanvas>(uint32_t e) { return HasUICanvas(e); }
		template<> inline bool HasComponent<Component::UIImage>(uint32_t e) { return HasUIImage(e); }

		template<> inline bool HasComponent<Component::Renderer>(uint32_t e) { return HasRenderer(e); }
		template<> inline bool HasComponent<Component::Light>(uint32_t e) { return HasLight(e); }
		template<> inline bool HasComponent<Component::Rigidbody>(uint32_t e) { return HasRigidbody(e); }
		template<> inline bool HasComponent<Component::Collider>(uint32_t e) { return HasCollider(e); }
		template<> inline bool HasComponent<Component::NativeScript>(uint32_t e) { return HasScript(e); }
		template<> inline bool HasComponent<Component::Animator>(uint32_t e) { return HasAnimator(e); }
		template<> inline bool HasComponent<Component::Camera>(uint32_t e) { return HasCamera(e); }

		NANOENGINE_API uint32_t GetParent(uint32_t child);

		NANOENGINE_API const Core::LayerID GetLayer(Entity e);
		NANOENGINE_API const Core::LayerMask GetLayerBit(Entity e);
	}

	namespace Command {
		NANOENGINE_API uint32_t CreateEntity();
		NANOENGINE_API uint32_t CreateUICanvasEntity();
		NANOENGINE_API uint32_t CreateUIImageEntity(uint32_t parentCanvas);
		NANOENGINE_API void DestroyEntity(uint32_t e);
		NANOENGINE_API void SetParent(Entity _child, Entity _newParent, int _insertIndex, bool _keepWorldPos = true);
		NANOENGINE_API void SetActive(Entity entity, bool isActive);

		NANOENGINE_API void AddLightComponent(uint32_t e);
		NANOENGINE_API void AddRendererComponent(uint32_t e);
		NANOENGINE_API void AddRigidbodyComponent(uint32_t e);
		NANOENGINE_API void AddColliderComponent(uint32_t e);
		NANOENGINE_API void AddAudioSourceComponent(uint32_t e);
		NANOENGINE_API void AddScriptComponent(uint32_t e);
		NANOENGINE_API void AddCameraComponent(uint32_t e);

		NANOENGINE_API void RemoveLightComponent(uint32_t e);
		NANOENGINE_API void RemoveRendererComponent(uint32_t e);
		NANOENGINE_API void RemoveRigidbodyComponent(uint32_t e);
		NANOENGINE_API void RemoveColliderComponent(uint32_t e);
		NANOENGINE_API void RemoveAudioSourceComponent(uint32_t e);
		NANOENGINE_API void RemoveCameraComponent(uint32_t e);

		template <typename C>
		void AddComponent(Entity e) {
			NE::GetScene().GetECSCoordinator().AddComponent<C>(e, C{});
		}

		template <typename C>
		void AddComponent(Entity e, const C& component) {
			NE::GetScene().GetECSCoordinator().AddComponent<C>(e, component);
		}

		// --- Editor Component Mutators --- //
		NANOENGINE_API Component::EntityMeta& GetEntityMeta(uint32_t e);
		NANOENGINE_API Component::Transform& GetEntityTransform(uint32_t e);
		NANOENGINE_API Component::Renderer& GetEntityRenderer(uint32_t e);
		NANOENGINE_API Component::Light& GetEntityLight(uint32_t e);
		NANOENGINE_API Component::Rigidbody& GetEntityRigidbody(uint32_t e);
		NANOENGINE_API Component::Collider& GetEntityCollider(uint32_t e);
		NANOENGINE_API Component::AudioSource& GetEntityAudioSource(uint32_t e);
		NANOENGINE_API Component::NativeScript& GetEntityScript(uint32_t e);
		NANOENGINE_API Component::UIRectTransform& GetUIRectTransform(uint32_t e);
		NANOENGINE_API Component::UIImage& GetUIImage(uint32_t e);
		NANOENGINE_API Component::UICanvas& GetUICanvas(uint32_t e);
		NANOENGINE_API Component::Camera& GetEntityCamera(uint32_t e);
		NANOENGINE_API Component::Hierarchy& GetEntityHierarchy(uint32_t e);

		// --- Script Management ---
		NANOENGINE_API std::vector<std::string> GetRegisteredScriptNames();
		NANOENGINE_API bool SetEntityScript(uint32_t e, const std::string& scriptName);
		NANOENGINE_API void RemoveEntityScript(uint32_t e);
		NANOENGINE_API bool IsScriptRegistered(const std::string& scriptName);

		NANOENGINE_API void AddAnimatorComponent(uint32_t e);              // <-- ADD
		NANOENGINE_API Component::Animator& GetEntityAnimator(uint32_t e);

		NANOENGINE_API void SetLayer(Entity e, Core::LayerID layer);
	}

}
