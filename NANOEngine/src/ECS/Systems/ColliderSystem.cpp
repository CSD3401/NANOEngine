#include "ColliderSystem.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Collider.hpp"
#include "../../Physics/PhysicsManager.hpp"

namespace NANOEngine::ECS::Systems {

	ColliderSystem::ColliderSystem(ComponentManager* cm) : m_componentManager(cm)
	{
	}

	void ColliderSystem::OnEntityAdded(Entity entity)
	{

	}

	void ColliderSystem::OnEntityRemoved(Entity entity)
	{

	}

	void ColliderSystem::Init()
	{
	}

	void ColliderSystem::Update(double dt)
	{

	}

	void ColliderSystem::Exit()
	{
	}

}