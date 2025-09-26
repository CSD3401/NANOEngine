#include "ECSExports.hpp"

#include "../ECS/Components/EntityMeta.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../ECS/Components/Light.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/Collider.hpp"
#include "../ECS/Components/NativeScript.hpp"
#include "../ECS/Systems/ScriptSystem.hpp"
#include "../SceneManagement/Scene.hpp"

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

		const Component::NativeScript& GetEntityScript(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::NativeScript>(e);
		}
	}

	namespace Command {

		uint32_t CreateEntity() {
			return GetScene().GetECSCoordinator().CreateEntity();
		}

		void DestroyEntity(uint32_t e) {
			GetScene().GetECSCoordinator().DestroyEntity(e);
		}

		void AddLightComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Light{});
		}

		void AddRendererComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Renderer{});
		}

		void AddRigidbodyComponent(uint32_t e) {
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Rigidbody{});
		}

		void AddColliderComponent(uint32_t e) {
			if (GetScene().GetECSCoordinator().HasComponent<ECS::Component::Collider>(e))
				return;
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Collider{});
		}

		void AddScriptComponent(uint32_t e) {
			if (GetScene().GetECSCoordinator().HasComponent<ECS::Component::NativeScript>(e))
				return;
			GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::NativeScript{});
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

		Component::NativeScript& GetEntityScript(uint32_t e) {
			return NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::NativeScript>(e);
		}

		// === Script Management Implementation ===
		
		std::vector<std::string> GetRegisteredScriptNames() {
			auto* scriptSystem = GetScene().GetECSCoordinator().m_scriptSystem.get();
			if (scriptSystem && scriptSystem->GetScriptingEngine()) {
				return scriptSystem->GetScriptingEngine()->GetRegisteredScriptNames();
			}
			return {};
		}

		bool SetEntityScript(uint32_t e, const std::string& scriptName) {
			if (!GetScene().GetECSCoordinator().HasComponent<ECS::Component::NativeScript>(e)) {
				return false;
			}

			auto& script = GetEntityScript(e);
			auto* scriptSystem = GetScene().GetECSCoordinator().m_scriptSystem.get();
			
			if (scriptSystem && scriptSystem->GetScriptingEngine()) {
				auto factory = scriptSystem->GetScriptingEngine()->GetScriptFactory(scriptName);
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
			if (scriptSystem && scriptSystem->GetScriptingEngine()) {
				return scriptSystem->GetScriptingEngine()->IsScriptRegistered(scriptName);
			}
			return false;
		}
	}

}
