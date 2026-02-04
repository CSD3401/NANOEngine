#ifndef UI_TRANSFORM_SYSTEM_HPP
#define UI_TRANSFORM_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include <unordered_map>
#include <vector>

namespace NE::ECS::Systems {

    class UITransformSystem : public System {
    public:
        UITransformSystem(ComponentManager* cm);

        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

        void Init() override;
        void Update(double dt) override;
        void Exit() override;

        // parent management
        void SetParent(Entity child, Entity newParent);

        // get entity from luid
        Entity GetEntityFromLUID(uint64_t luid) const;

    private:
        struct PendingParent {
            Entity child;
            uint64_t parentLuid;
        };

        ComponentManager* m_cm;
        std::unordered_map<uint64_t, Entity> m_luidToEntity;
        std::vector<PendingParent> m_pendingParents;

        void ResolvePendingParents();
        void UpdateWorldTransforms(); // kiv
    };

} // namespace NE::ECS::Systems

#endif // UI_TRANSFORM_SYSTEM_HPP
