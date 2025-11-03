#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include "../ECS/Core/Entity.hpp"
#include "../Math/Vec3.hpp"

namespace NE::Physics
{
	struct CollisionInfo
	{
		NE::ECS::Entity entityA;
		NE::ECS::Entity entityB;
		Math::Vec3 contactPoint;
		Math::Vec3 contactNormal;
		float impulse;
	};

	class PhysicsContactListener : public JPH::ContactListener
	{
	public:
		using CollisionCallback = std::function<void(const CollisionInfo&)>;

		// Callback registration (similar to Unity's event system)
		void RegisterCollisionEnterCallback(CollisionCallback callback)
		{
			m_OnCollisionEnter = callback;
		}

		void RegisterCollisionStayCallback(CollisionCallback callback)
		{
			m_OnCollisionStay = callback;
		}

		void RegisterCollisionExitCallback(CollisionCallback callback)
		{
			m_OnCollisionExit = callback;
		}

		// Jolt Physics callbacks
		virtual JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1,
			const JPH::Body& inBody2,
			JPH::RVec3Arg inBaseOffset,
			const JPH::CollideShapeResult& inCollisionResult) override;

		virtual void OnContactAdded(const JPH::Body& inBody1,
			const JPH::Body& inBody2,
			const JPH::ContactManifold& inManifold,
			JPH::ContactSettings& ioSettings) override;

		virtual void OnContactPersisted(const JPH::Body& inBody1,
			const JPH::Body& inBody2,
			const JPH::ContactManifold& inManifold,
			JPH::ContactSettings& ioSettings) override;

		virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;

		// Entity mapping Jolt bodies - ECS entities
		void MapBodyToEntity(JPH::BodyID bodyID, NE::ECS::Entity entity);
		void UnmapBody(JPH::BodyID bodyID);
		NE::ECS::Entity GetEntityFromBody(JPH::BodyID bodyID) const;

		// Call this AFTER your physics step each frame
		void UpdateCollisionStates();

	private:
		CollisionCallback m_OnCollisionEnter;
		CollisionCallback m_OnCollisionStay;
		CollisionCallback m_OnCollisionExit;

		// Store mapping by body INDEX, not full BodyID (Jolt uses indices in collision callbacks)
		std::unordered_map<uint32_t, NE::ECS::Entity> m_BodyToEntityMap;

		// Changed: Track which collisions are active (bool instead of count)
		std::unordered_map<uint64_t, bool> m_CurrentCollisions;   // Active this frame
		std::unordered_map<uint64_t, bool> m_PreviousCollisions;  // Active last frame

		uint64_t GetCollisionPairKey(JPH::BodyID body1, JPH::BodyID body2);
	};
}