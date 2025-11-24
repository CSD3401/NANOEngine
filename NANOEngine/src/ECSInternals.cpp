#include "ECSInternals.hpp"
#include "SceneManagement/Scene.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/Renderer.hpp"
#include "ECS/Components/Light.hpp"
#include "ECS/Components/Rigidbody.hpp"
#include "ECS/Components/Collider.hpp"
//#include "AssetManager.hpp"
#include "Physics/PhysicsManager.hpp"

namespace NE {
	SceneManagement::Scene& GetScene();
	//extern SceneManagement::Scene scene;
	void SetMotionType(uint32_t bodyid, uint8_t motionType)
	{
		Physics::PhysicsManager::SetMotionType(bodyid, static_cast<JPH::EMotionType>(motionType));
	}


}