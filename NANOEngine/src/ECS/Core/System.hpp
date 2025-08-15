#ifndef NANOENGINE_ECS_SYSTEM_HPP
#define NANOENGINE_ECS_SYSTEM_HPP

#include "SparseSet.hpp"
#include "Entity.hpp"

namespace NE::ECS {

    class System {
    public:
        SparseSet<Entity, MAX_ENTITIES> m_entities;

        virtual ~System() = default;

        const auto& GetEntities() { return m_entities.GetDenseContainer(); }

        virtual void OnEntityAdded(Entity entity) = 0;
        virtual void OnEntityRemoved(Entity entity) = 0;

        virtual void Init() = 0;
        virtual void Update(double deltaTime) = 0; // override in concrete systems
        virtual void Exit() = 0;
    };

}

#endif // !NANOENGINE_ECS_SYSTEM_HPP
