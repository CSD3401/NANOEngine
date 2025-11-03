#include "PhysicsContactListener.hpp"
#include <Jolt/Physics/Body/Body.h>
#include <iostream>

namespace NE::Physics
{
	JPH::ValidateResult PhysicsContactListener::OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult)
	{
		(void)inBody1;
		(void)inBody2;
		(void)inCollisionResult;
		(void)inBaseOffset;
		return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	void PhysicsContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
	{
		(void)ioSettings;

		JPH::BodyID body1ID = inBody1.GetID();
		JPH::BodyID body2ID = inBody2.GetID();

		// Filter out self-collisions
		if (body1ID == body2ID)
		{
			return;
		}

		uint64_t collisionKey = GetCollisionPairKey(body1ID, body2ID);

		// Mark this collision as active this frame
		m_CurrentCollisions[collisionKey] = true;

		std::cout << "OnContactAdded: Body " << body1ID.GetIndex() << " vs Body " << body2ID.GetIndex()
			<< " | Key: " << collisionKey << std::endl;

		// Check if this is a NEW collision (wasn't in previous frame)
		if (m_PreviousCollisions.find(collisionKey) == m_PreviousCollisions.end())
		{
			NE::ECS::Entity entity1 = GetEntityFromBody(body1ID);
			NE::ECS::Entity entity2 = GetEntityFromBody(body2ID);

			std::cout << "  -> COLLISION ENTER: Entity " << entity1 << " with Entity " << entity2 << std::endl;

			// Trigger collision enter
			if (m_OnCollisionEnter)
			{
				CollisionInfo info;
				info.entityA = entity1;
				info.entityB = entity2;

				// Get collision point and normal from the first contact point
				if (inManifold.mRelativeContactPointsOn1.size() > 0)
				{
					JPH::Vec3 worldPoint = inBody1.GetCenterOfMassPosition() + inManifold.mRelativeContactPointsOn1[0];
					info.contactPoint = Math::Vec3(worldPoint.GetX(), worldPoint.GetY(), worldPoint.GetZ());
					info.contactNormal = Math::Vec3(inManifold.mWorldSpaceNormal.GetX(), inManifold.mWorldSpaceNormal.GetY(), inManifold.mWorldSpaceNormal.GetZ());
				}

				m_OnCollisionEnter(info);
			}
		}
	}

	void PhysicsContactListener::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
	{
		(void)ioSettings;

		JPH::BodyID body1ID = inBody1.GetID();
		JPH::BodyID body2ID = inBody2.GetID();

		// Filter out self-collisions
		if (body1ID == body2ID)
		{
			return;
		}

		uint64_t collisionKey = GetCollisionPairKey(body1ID, body2ID);

		// Mark this collision as active this frame
		m_CurrentCollisions[collisionKey] = true;

		NE::ECS::Entity entity1 = GetEntityFromBody(body1ID);
		NE::ECS::Entity entity2 = GetEntityFromBody(body2ID);

		// Additional safety check
		if (entity1 == entity2 && entity1 != NE::ECS::Entity{ 0 })
		{
			return;
		}

		if (m_OnCollisionStay)
		{
			CollisionInfo info;
			info.entityA = entity1;
			info.entityB = entity2;

			if (inManifold.mRelativeContactPointsOn1.size() > 0)
			{
				JPH::Vec3 worldPoint = inBody1.GetCenterOfMassPosition() + inManifold.mRelativeContactPointsOn1[0];
				info.contactPoint = Math::Vec3(worldPoint.GetX(), worldPoint.GetY(), worldPoint.GetZ());
				info.contactNormal = Math::Vec3(inManifold.mWorldSpaceNormal.GetX(), inManifold.mWorldSpaceNormal.GetY(), inManifold.mWorldSpaceNormal.GetZ());
			}

			m_OnCollisionStay(info);
		}
	}

	void PhysicsContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
	{
		// We don't need to do anything here anymore!
		// The collision lifecycle is handled in UpdateCollisionStates()
		(void)inSubShapePair;
	}

	void PhysicsContactListener::MapBodyToEntity(JPH::BodyID bodyID, NE::ECS::Entity entity)
	{
		// Store using just the index, since that's what Jolt uses in collision callbacks
		uint32_t index = bodyID.GetIndex();
		m_BodyToEntityMap[index] = entity;
		std::cout << "MapBodyToEntity: Index " << index << " -> Entity " << entity << std::endl;
	}

	void PhysicsContactListener::UnmapBody(JPH::BodyID bodyID)
	{
		uint32_t index = bodyID.GetIndex();
		std::cout << "UnmapBody called for BodyID index: " << index << std::endl;

		m_BodyToEntityMap.erase(index);

		// Clean up any collisions involving this body
		std::vector<uint64_t> collisionsToRemove;

		for (const auto& [collisionKey, _] : m_CurrentCollisions)
		{
			uint32_t id1 = static_cast<uint32_t>(collisionKey >> 32);
			uint32_t id2 = static_cast<uint32_t>(collisionKey & 0xFFFFFFFF);

			if (id1 == index || id2 == index)
			{
				collisionsToRemove.push_back(collisionKey);
			}
		}

		for (const auto& keyToRemove : collisionsToRemove)
		{
			m_CurrentCollisions.erase(keyToRemove);
			m_PreviousCollisions.erase(keyToRemove);
		}

		std::cout << "UnmapBody completed. Current collision count: " << m_CurrentCollisions.size() << std::endl;
	}

	NE::ECS::Entity PhysicsContactListener::GetEntityFromBody(JPH::BodyID bodyID) const
	{
		uint32_t index = bodyID.GetIndex();
		auto it = m_BodyToEntityMap.find(index);
		if (it != m_BodyToEntityMap.end()) {
			return it->second;
		}
		std::cout << "WARNING: GetEntityFromBody - No entity found for body index " << index << std::endl;
		return NE::ECS::Entity{ 0 };
	}

	uint64_t PhysicsContactListener::GetCollisionPairKey(JPH::BodyID body1, JPH::BodyID body2)
	{
		uint32_t id1 = body1.GetIndex();
		uint32_t id2 = body2.GetIndex();

		// Ensure consistent ordering - always put smaller ID first
		if (id1 < id2)
		{
			return ((uint64_t)id1 << 32) | id2;
		}
		else
		{
			return ((uint64_t)id2 << 32) | id1;
		}
	}

	void PhysicsContactListener::UpdateCollisionStates()
	{
		// Called once per frame AFTER physics step

		// Check for collisions that ended (were in previous frame but not current)
		for (const auto& [collisionKey, _] : m_PreviousCollisions)
		{
			if (m_CurrentCollisions.find(collisionKey) == m_CurrentCollisions.end())
			{
				// This collision existed last frame but not this frame - it ended!
				std::cout << "Collision ended for key: " << collisionKey << std::endl;

				if (m_OnCollisionExit)
				{
					// Extract body INDICES from the collision key
					uint32_t index1 = static_cast<uint32_t>(collisionKey >> 32);
					uint32_t index2 = static_cast<uint32_t>(collisionKey & 0xFFFFFFFF);

					std::cout << "  Extracted indices: " << index1 << ", " << index2 << std::endl;

					// Create BodyIDs using the indices
					JPH::BodyID bodyID1(index1);
					JPH::BodyID bodyID2(index2);

					NE::ECS::Entity entity1 = GetEntityFromBody(bodyID1);
					NE::ECS::Entity entity2 = GetEntityFromBody(bodyID2);

					std::cout << "  Resolved to entities: " << entity1 << ", " << entity2 << std::endl;

					// Only trigger if both entities are valid
					if (entity1 != NE::ECS::Entity{ 0 } && entity2 != NE::ECS::Entity{ 0 })
					{
						CollisionInfo info;
						info.entityA = entity1;
						info.entityB = entity2;
						m_OnCollisionExit(info);
					}
				}
			}
		}

		// Swap current into previous for next frame
		m_PreviousCollisions = m_CurrentCollisions;

		// Clear current collisions for next frame
		m_CurrentCollisions.clear();
	}
}