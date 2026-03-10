#include "pch.h"
#include "ContactListenerImpl.hpp"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include "PhysicsManager.hpp"
#include "ContactDefs.hpp"

namespace NE::Physics {
    namespace {
        bool IsTriggerContact(const JPH::Body& b1, const JPH::Body& b2) {
            const JPH::Shape* s1 = b1.GetShape();
            const JPH::Shape* s2 = b2.GetShape();
            return (s1 && b1.IsSensor()) || (s2 && b2.IsSensor());
        }
    }

    ContactListenerImpl::ContactListenerImpl(PhysicsManager* pm)
        : m_pm(pm) { }

    void ContactListenerImpl::OnContactAdded(
        const JPH::Body& body1,
        const JPH::Body& body2,
        const JPH::ContactManifold& /*manifold*/,
        JPH::ContactSettings&
    ) {
        if (!m_pm) return;

        const uint64_t a = m_pm->BodyToLuid(body1.GetID());
        const uint64_t b = m_pm->BodyToLuid(body2.GetID());
        if (a == 0 || b == 0) return;

        RawContactEvent e;
        e.aLuid = a;
        e.bLuid = b;
        e.isTrigger = IsTriggerContact(body1, body2);
        e.type = ContactEventType::Added;

        m_pm->PushRawContactEvent(e);
    }

    void ContactListenerImpl::OnContactPersisted(
        const JPH::Body& body1,
        const JPH::Body& body2,
        const JPH::ContactManifold& /*manifold*/,
        JPH::ContactSettings&
    ) {
        if (!m_pm) return;

        const uint64_t a = m_pm->BodyToLuid(body1.GetID());
        const uint64_t b = m_pm->BodyToLuid(body2.GetID());
        if (a == 0 || b == 0) return;

        RawContactEvent e;
        e.aLuid = a;
        e.bLuid = b;
        e.isTrigger = IsTriggerContact(body1, body2);
        e.type = ContactEventType::Persisted;

        m_pm->PushRawContactEvent(e);
    }

    void ContactListenerImpl::OnContactRemoved(
        const JPH::SubShapeIDPair& pair
    ) {
        if (!m_pm) return;

        const uint64_t a = m_pm->BodyToLuid(pair.GetBody1ID());
        const uint64_t b = m_pm->BodyToLuid(pair.GetBody2ID());
        if (a == 0 || b == 0) return;
    }
}
