#pragma once

#include <cstdint>
#include <string>
#include "../NANOEngineAPI.hpp"
#include "../ECS/Core/Entity.hpp"
#include "Math/Vec3.hpp"

// Forward Decl
namespace NE::ECS::Component 
{
	struct Collider;
	struct Transform;
}

namespace NE::Physics 
{

	namespace Query 
	{
		NANOENGINE_API bool HasPhysicsBody(uint32_t entity);
		NANOENGINE_API uint32_t GetPhysicsBodyId(uint32_t entity);
		NANOENGINE_API void GetPhysicsTransform(uint32_t entity, NE::Math::Vec3& position, NE::Math::Vec3& rotation);
		NANOENGINE_API void GetTransform(uint32_t index, Math::Vec3& position, Math::Vec3& rotation);
	}

	namespace Command 
	{
		/*
		        static void Init();
        static void Update(float dt);
        static void Shutdown();
		*/
		NANOENGINE_API void Init();
		NANOENGINE_API void Update(float dt);
		NANOENGINE_API void Shutdown();

		NANOENGINE_API void ActivateBodies();
		NANOENGINE_API void DeactivateBodies();
		NANOENGINE_API void DestroyBody(uint32_t index);
		NANOENGINE_API void RegisterEntityBody(uint32_t entity, uint32_t bodyID);
		NANOENGINE_API void UnregisterEntityBody(uint32_t entity);

		NANOENGINE_API void CreatePhysicsBody(uint32_t entity);
		NANOENGINE_API void UpdatePhysicsBody(uint32_t entity);
		NANOENGINE_API void RemovePhysicsBody(uint32_t entity);
		NANOENGINE_API void SetPhysicsMotionType(uint32_t entity, int motionType);
	}



}
