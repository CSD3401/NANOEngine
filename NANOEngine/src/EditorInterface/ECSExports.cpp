#include "ECSExports.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../ECS/Components/Light.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/Collider.hpp"

namespace NANOEngine {
	SceneManagement::Scene& GetScene();
}

namespace NE::ECS {

	namespace Query {
		const NANOEngine::ECS::Component::Transform& GetEntityTransform(uint32_t e) {
			return NANOEngine::GetScene().GetECSCoordinator().GetComponent<NANOEngine::ECS::Component::Transform>(e);
		}

		const NANOEngine::ECS::Component::Renderer& GetEntityRenderer(uint32_t e) {
			return NANOEngine::GetScene().GetECSCoordinator().GetComponent<NANOEngine::ECS::Component::Renderer>(e);
		}

		const NANOEngine::ECS::Component::Light& GetEntityLight(uint32_t e) {
			return NANOEngine::GetScene().GetECSCoordinator().GetComponent<NANOEngine::ECS::Component::Light>(e);
		}

		const NANOEngine::ECS::Component::Rigidbody& GetEntityRigidbody(uint32_t e) {
			return NANOEngine::GetScene().GetECSCoordinator().GetComponent<NANOEngine::ECS::Component::Rigidbody>(e);
		}

		const NANOEngine::ECS::Component::Collider& GetEntityCollider(uint32_t e) {
			return NANOEngine::GetScene().GetECSCoordinator().GetComponent<NANOEngine::ECS::Component::Collider>(e);
		}
	}

	namespace Command {

	}

}
