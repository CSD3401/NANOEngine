#include "PhysicsExports.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Components/EntityMeta.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Collider.hpp"
#include "../Physics/PhysicsManager.hpp"
#include "Engine.hpp"


//namespace NE {
//	SceneManagement::Scene& GetScene();
//}

namespace NE::Physics 
{

	namespace Query {

	}

	namespace Command {
		void DrawSelectedCollider(ECS::Entity e) {
			auto& meta = GetScene().GetECSCoordinator().GetComponent<ECS::Component::EntityMeta>(e);
			auto& t = GetScene().GetECSCoordinator().GetComponent<ECS::Component::Transform>(e);
			auto& col = GetScene().GetECSCoordinator().GetComponent<ECS::Component::Collider>(e);

			if (col.type != ECS::Component::Collider::ColliderType::Mesh)
				Physics::PhysicsManager::GetInstance().DrawShapeGizmo(meta.luid, t, col);
		}
	}


}
