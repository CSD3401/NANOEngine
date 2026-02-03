#include "HierarchySystem.hpp"

#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"
#include "Math/Mat4.hpp"
#include "../Components/Hierarchy.hpp"
#include "../Components/Transform.hpp"
#include "../Components/EntityMeta.hpp"
#include <Core/Profiler.hpp>

namespace NE::ECS::Systems {

    namespace {
        NE::Math::Mat4 InverseTRS(const NE::Math::Mat4& world) {
        	using namespace NE::Math;
        	const Vec3 p = world.GetTranslation();
        	const Vec3 r = world.GetRotation(); // Euler (same order you use to build)
        	const Vec3 s = world.GetScale();

        	// (T*R*S)^-1 = S^-1 * R^-1 * T^-1
        	Mat4 invT = Mat4::BuildTranslation(Vec3{ -p.x, -p.y, -p.z });
        	Mat4 invR = Mat4::BuildZRotation(-r.z) * Mat4::BuildYRotation(-r.y) * Mat4::BuildXRotation(-r.x);
        	Mat4 invS = Mat4::BuildScaling(1.0f / s.x, 1.0f / s.y, 1.0f / s.z);
        	return invS * invR * invT;
        }

        void DecomposeToTRS(const NE::Math::Mat4& m,
        	NE::Math::Vec3& outPos,
        	NE::Math::Vec3& outRot,
        	NE::Math::Vec3& outScale)
        {
        	outPos = m.GetTranslation();
        	outRot = m.GetRotation();
        	outScale = m.GetScale();
        }
    }

	HierarchySystem::HierarchySystem(ComponentManager* cm, Core::LUIDRegistry* lr) 
        : m_componentManager(cm), m_luidRegistry(lr) {}

	void HierarchySystem::OnEntityAdded(Entity e) {
		auto& h = m_componentManager->GetComponent<Component::Hierarchy>(e);
		auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);

		if (h.luid != 0) {
			m_luidToEntity[h.luid] = e;
        } else {
            h.luid = Core::LUIDGenerator::Generate("hr");
        }
		m_luidRegistry->Register(h.luid, &h, e);

        if (meta.luid != 0) {
            m_luidToEntity[meta.luid] = e;
        } else {
            meta.luid = Core::LUIDGenerator::Generate("em");
        }
		m_luidRegistry->Register(meta.luid, &meta, e);

		if (h.parentLuid != 0) {
			m_pendingParents.push_back({ e, h.parentLuid });
		}
	}

	void HierarchySystem::OnEntityRemoved(Entity e) {
        auto& hier = m_componentManager->GetComponent<Component::Hierarchy>(e);
        auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);

        m_luidRegistry->Unregister(hier.luid);
        m_luidRegistry->Unregister(meta.luid);
	}

	void HierarchySystem::Init() {
        ResolvePendingParentsForAll(false);
	}

	void HierarchySystem::Update(double) {
#ifndef PRODUCTION_BUILD
        NE_PROFILE_FUNCTION();
#endif

		if (m_pendingParents.size() > 0)
            ResolvePendingParentsForAll(false);
	}

	void HierarchySystem::Exit() {
	}

	void HierarchySystem::SetParent(Entity child, Entity newParent, bool keepWorld) {
        auto& childH = m_componentManager->GetComponent<Component::Hierarchy>(child);

        // Check if entity has Transform component (UI entities may not have Transform)
        bool hasTransform = m_componentManager->HasComponent<Component::Transform>(child);

        NE::Math::Mat4 childWorldBefore;
        if (keepWorld && hasTransform) {
            auto& childT = m_componentManager->GetComponent<Component::Transform>(child);
            childWorldBefore = childT.worldMatrix;
        }

        if (childH.parent != Component::INVALID_PARENT) {
            auto& oldParentH = m_componentManager->GetComponent<Component::Hierarchy>(childH.parent);
            auto& vec = oldParentH.children;
            vec.erase(std::remove(vec.begin(), vec.end(), child), vec.end());
        }

        childH.parent = (newParent == Component::INVALID_PARENT)
            ? Component::INVALID_PARENT
            : newParent;

        if (newParent != Component::INVALID_PARENT) {
            auto& parentH = m_componentManager->GetComponent<Component::Hierarchy>(newParent);
            parentH.children.push_back(child);

            childH.parentLuid = parentH.luid;
        } else {
            childH.parentLuid = 0;
        }

        if (keepWorld && hasTransform) {
            auto& childT = m_componentManager->GetComponent<Component::Transform>(child);
            NE::Math::Mat4 localM;
            if (newParent != Component::INVALID_PARENT && m_componentManager->HasComponent<Component::Transform>(newParent)) {
                auto& parentT = m_componentManager->GetComponent<Component::Transform>(newParent);
                NE::Math::Mat4 invParent = InverseTRS(parentT.worldMatrix);
                localM = invParent * childWorldBefore;
            } else {
                localM = childWorldBefore;
            }

            DecomposeToTRS(localM,
                childT.localPosition,
                childT.localRotationEuler,
                childT.localScale);
            childT.isDirty = true;
        }
	}

    void HierarchySystem::SetParent(Entity child,
        Entity newParent,
        int insertIndex,
        bool keepWorld)
    {
        using Component::INVALID_PARENT;

        auto& childH = m_componentManager->GetComponent<Component::Hierarchy>(child);

        // Check if entity has Transform component (UI entities may not have Transform)
        bool hasTransform = m_componentManager->HasComponent<Component::Transform>(child);

        const uint32_t oldParent = childH.parent;

        NE::Math::Mat4 childWorldBefore;
        if (keepWorld && hasTransform) {
            auto& childT = m_componentManager->GetComponent<Component::Transform>(child);
            childWorldBefore = childT.worldMatrix;
        }

        int oldIndex = -1;
        if (oldParent != INVALID_PARENT) {
            auto& oldParentH = m_componentManager->GetComponent<Component::Hierarchy>(oldParent);
            auto& vec = oldParentH.children;

            for (int i = 0; i < static_cast<int>(vec.size()); ++i) {
                if (vec[i] == child) {
                    oldIndex = i;
                    vec.erase(vec.begin() + i);
                    break;
                }
            }
        }

        childH.parent = (newParent == INVALID_PARENT)
            ? INVALID_PARENT
            : newParent;

        if (newParent != INVALID_PARENT) {
            auto& parentH = m_componentManager->GetComponent<Component::Hierarchy>(newParent);
            auto& vec = parentH.children;

            if (newParent == oldParent && oldIndex != -1 && insertIndex > oldIndex) {
                --insertIndex;
            }

            if (insertIndex < 0 || insertIndex > static_cast<int>(vec.size())) {
                insertIndex = static_cast<int>(vec.size());
            }

            vec.insert(vec.begin() + insertIndex, child);

            childH.parentLuid = parentH.luid;
        } else {
            childH.parentLuid = 0;
        }

        if (keepWorld && hasTransform) {
            auto& childT = m_componentManager->GetComponent<Component::Transform>(child);
            NE::Math::Mat4 localM;
            if (newParent != INVALID_PARENT && m_componentManager->HasComponent<Component::Transform>(newParent)) {
                auto& parentT = m_componentManager->GetComponent<Component::Transform>(newParent);
                NE::Math::Mat4 invParent = InverseTRS(parentT.worldMatrix);
                localM = invParent * childWorldBefore;
            } else {
                localM = childWorldBefore;
            }

            DecomposeToTRS(localM,
                childT.localPosition,
                childT.localRotationEuler,
                childT.localScale);
            childT.isDirty = true;
        }
    }

    void HierarchySystem::SetActive(Entity root, bool isActive) {
        m_componentManager->GetComponent<Component::EntityMeta>(root).isActive = isActive;
        auto& hier = m_componentManager->GetComponent<Component::Hierarchy>(root);

        for (auto child : hier.children)
            SetActive(child, isActive);
    }

    //void HierarchySystem::SetParent(Entity child,
    //    Entity newParent,
    //    bool keepWorld)
    //{
    //    SetParent(child, newParent, /*insertIndex*/ std::numeric_limits<int>::max(), keepWorld);
    //}

	void HierarchySystem::ResolvePendingParentsForAll(bool keepWorldForNewParents) {
        std::vector<PendingParent> stillPending;
        stillPending.reserve(m_pendingParents.size());

        for (const PendingParent& pp : m_pendingParents) {
            if (!m_componentManager->HasComponent<Component::Hierarchy>(pp.child))
                continue;

            //auto& childH = m_componentManager->GetComponent<Component::Hierarchy>(pp.child);

            auto it = m_luidToEntity.find(pp.parentLuid);
            if (it != m_luidToEntity.end()) {
                Entity parentEnt = it->second;
                SetParent(pp.child, parentEnt, keepWorldForNewParents);
            } else {
                stillPending.push_back(pp);
            }
        }

        m_pendingParents.swap(stillPending);
	}
}