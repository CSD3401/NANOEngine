#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>

namespace NE::Physics {
    class PhysicsManager;

	class ContactListenerImpl final : public JPH::ContactListener {
    public:
		explicit ContactListenerImpl(PhysicsManager* pm);

        void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
            const JPH::ContactManifold& manifold, JPH::ContactSettings&) override;

        void OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2,
            const JPH::ContactManifold& manifold, JPH::ContactSettings&) override;

        void OnContactRemoved(const JPH::SubShapeIDPair& pair) override;

    private:
        PhysicsManager* m_pm = nullptr;
	};
}
