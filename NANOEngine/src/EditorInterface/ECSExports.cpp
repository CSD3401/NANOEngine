#include "ECSExports.hpp"

#include "../ECS/Components/EntityMeta.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../ECS/Components/Light.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/Collider.hpp"
#include "../ECS/Components/AudioSource.hpp"
#include "../ECS/Components/NativeScript.hpp"
#include "../ECS/Components/Camera.hpp"
#include "../ECS/Systems/ScriptSystem.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Components/Animator.hpp"
#include "Scripting/ScriptingEngine.hpp"
#include "Core/LUIDGenerator.hpp"
#include "ECS/Systems/TransformSystem.hpp"




namespace NE {
	SceneManagement::Scene& GetScene();
}

namespace NE::ECS {

	namespace Query {

		std::unordered_map<std::type_index, uint8_t> GetRegisteredComponentTypes() {
			return GetScene().GetECSCoordinator().GetRegisteredComponentTypes();
		}

		uint64_t GetEntitySignature(uint32_t e) {
			return GetScene().GetECSCoordinator().GetSignature(e).to_ullong();
		}

		const Component::EntityMeta& GetEntityMeta(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::EntityMeta>(e);
		}

		const Component::Transform& GetEntityTransform(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Transform>(e);
		}

		const Component::Renderer& GetEntityRenderer(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
		}

		const Component::Light& GetEntityLight(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Light>(e);
		}

		const Component::Rigidbody& GetEntityRigidbody(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Rigidbody>(e);
		}

		const Component::Collider& GetEntityCollider(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Collider>(e);
		}

		const Component::AudioSource& GetEntityAudioSource(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::AudioSource>(e);
		}

		const Component::NativeScript& GetEntityScript(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::NativeScript>(e);
		}

		// Component existence checks
		bool HasTransform(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Transform>(e);
		}

		bool HasRenderer(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Renderer>(e);
		}

		bool HasLight(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Light>(e);
		}

		bool HasRigidbody(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Rigidbody>(e);
		}

		bool HasCollider(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Collider>(e);
		}

		bool HasAudioSource(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::AudioSource>(e);
		}

		bool HasScript(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::NativeScript>(e);
		}

		bool HasAnimator(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Animator>(e);
		}

		bool HasCamera(uint32_t e) {
			return GetScene().GetECSCoordinator().HasComponent<ECS::Component::Camera>(e);
		}

		const Component::Animator& GetEntityAnimator(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Animator>(e);
		}
		
		const Component::Camera& GetEntityCamera(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Camera>(e);
		}
	}

	namespace Command {

		uint32_t CreateEntity() {
			uint32_t newEntity = GetScene().GetECSCoordinator().CreateEntity();
			GetScene().GetECSCoordinator().AddComponent(
				newEntity, 
				Component::EntityMeta{ .name = "Unnamed Entity", .luid = Core::LUIDGenerator::Generate("en") });

			GetScene().GetECSCoordinator().AddComponent(
				newEntity, 
				Component::Transform{ .luid = Core::LUIDGenerator::Generate("tr") });

			return newEntity;
		}

		void DestroyEntity(uint32_t e) {
			GetScene().GetECSCoordinator().DestroyEntity(e);
		}

		void AddLightComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Light{ .luid = Core::LUIDGenerator::Generate("tr") });
		}

		void AddRendererComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Renderer{ .luid = Core::LUIDGenerator::Generate("re") });
		}

		void AddRigidbodyComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Rigidbody{ .luid = Core::LUIDGenerator::Generate("ri") });
		}

		void AddColliderComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Collider{});
		}

		void AddAudioSourceComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::AudioSource{});
		}

		void AddScriptComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::NativeScript{ .luid = Core::LUIDGenerator::Generate("sc") });
		}

		void AddCameraComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Camera{ .luid = Core::LUIDGenerator::Generate("ca") });
		}

		Component::EntityMeta& GetEntityMeta(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::EntityMeta>(e);
		}

		Component::Transform& GetEntityTransform(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Transform>(e);
		}

		Component::Renderer& GetEntityRenderer(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
		}
		
		Component::Light& GetEntityLight(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Light>(e);
		}

		Component::Rigidbody& GetEntityRigidbody(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Rigidbody>(e);
		}

		Component::Collider& GetEntityCollider(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Collider>(e);
		}

		Component::AudioSource& GetEntityAudioSource(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::AudioSource>(e);
		}
		
		Component::NativeScript& GetEntityScript(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::NativeScript>(e);
		}

		Component::Camera& GetEntityCamera(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Camera>(e);
		}

		void SetParent(uint32_t child, uint32_t parent, bool worldPositionStays) {
			NE::GetScene().GetECSCoordinator().m_transformSystem->SetParent(child, parent);
		}	

		uint32_t GetParent(uint32_t child) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Transform>(child).parent;
		}

		// === Script Management Implementation ===
		
		std::vector<std::string> GetRegisteredScriptNames() {
			auto* scriptSystem = GetScene().GetECSCoordinator().m_scriptSystem.get();
			if (scriptSystem) {
				return Scripting::ScriptingEngine::GetInstance().GetRegisteredScriptNames();
			}
			return {};
		}

		bool SetEntityScript(uint32_t e, const std::string& scriptName) {
			if (!GetScene().GetECSCoordinator().HasComponent<ECS::Component::NativeScript>(e)) {
				return false;
			}

			auto& script = GetEntityScript(e);
			auto* scriptSystem = GetScene().GetECSCoordinator().m_scriptSystem.get();
			
			if (scriptSystem) {
				auto factory = Scripting::ScriptingEngine::GetInstance().GetScriptFactory(scriptName);
				if (factory) {
					// Clean up existing script if any
					if (script.Instance && script.DestroyScript) {
						script.DestroyScript(script.Instance);
					} else if (script.Instance) {
						delete script.Instance;
					}

					// Set new script
					script.ScriptName = scriptName;
					script.CreateScript = factory;
					script.DestroyScript = [](IScript* instance) { delete instance; };
					script.Instance = nullptr; // Will be created by ScriptSystem
					scriptSystem->OnEntityAdded(e); // Force initialization
					return true;
				}
			}
			return false;
		}

		void RemoveEntityScript(uint32_t e) {
			if (!GetScene().GetECSCoordinator().HasComponent<ECS::Component::NativeScript>(e)) {
				return;
			}

			auto& script = GetEntityScript(e);
			
			// Clean up existing script
			if (script.Instance && script.DestroyScript) {
				script.DestroyScript(script.Instance);
			} else if (script.Instance) {
				delete script.Instance;
			}

			// Reset component
			script.ScriptName.clear();
			script.Instance = nullptr;
			script.CreateScript = nullptr;
			script.DestroyScript = nullptr;
		}

		bool IsScriptRegistered(const std::string& scriptName) {
			auto* scriptSystem = GetScene().GetECSCoordinator().m_scriptSystem.get();
			if (scriptSystem) {
				return Scripting::ScriptingEngine::GetInstance().IsScriptRegistered(scriptName);
			}
			return false;
		}

		void AddAnimatorComponent(uint32_t e) {
			if (GetScene().GetECSCoordinator().HasComponent<ECS::Component::Animator>(e))
				return;
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Animator{});
		}

		Component::Animator& GetEntityAnimator(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Animator>(e);
		}
	}

}
