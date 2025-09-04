#pragma once

#include "../ECS/Core/Entity.hpp"
#include "../ECS/Systems/ScriptSystem.hpp"
#include "../Core/API.hpp"

// Forward-declare the Entity class from your ECS so the script can know
// which entity it's attached to.
using Entity = NE::ECS::Entity;

namespace NE::Scripting {
    class ENGINE_API IScript {
    public:
        virtual ~IScript() = default;

        // The script instance is given a handle to its owner entity.
        void SetEntity(Entity entity) {
            m_Entity = entity;
        }

    protected:
        // Lifecycle functions to be overridden by user scripts.
        // They are not pure virtual, so users only need to implement what they need.
        virtual void OnCreate() {}
        virtual void OnUpdate(double deltaTime) { deltaTime; }
        virtual void OnDestroy() {}

        // A handle to the entity this script is attached to.
        Entity m_Entity;

    private:
        // Make the ScriptSystem a friend so it can call the private lifecycle methods.
        // This prevents anyone else from calling them accidentally.
        friend class NE::ECS::Systems::ScriptSystem;
    };
}


