/**
 * @file ScriptContext.hpp
 * @brief Internal script context definition (NOT part of public SDK)
 *
 * This is an INTERNAL ENGINE HEADER - scripts should NOT include this.
 * It provides the implementation details for the opaque ScriptContext
 * referenced in the public SDK API.
 */

#pragma once

#include "../ECS/Core/ComponentManager.hpp"
#include "../ECS/Core/EntityManager.hpp"
#include "../Physics/PhysicsManager.hpp"

namespace NE::Core {
    class LUIDRegistry;
}

// Forward declaration
namespace NE::Scripting {
    class ScriptingEngine;
}

namespace NE {
namespace Scripting {

    /**
     * @class ScriptContext
     * @brief Internal context that provides scripts access to engine systems
     *
     * This class is PIMPL (Pointer to Implementation) for the IScript class.
     * Scripts receive an opaque pointer to this, allowing the engine to
     * provide functionality without exposing implementation details.
     */
    class ScriptContext {
    public:
        ECS::ComponentManager* componentManager = nullptr;
        ECS::EntityManager* entityManager = nullptr;
        Core::LUIDRegistry* luidRegistry = nullptr;  // LUID registry for component lookups
        ScriptingEngine* scriptingEngine = nullptr;  // Script engine for script-to-script communication
        // Note: PhysicsManager is static, accessed directly via static methods

        ScriptContext() = default;

        explicit ScriptContext(ECS::ComponentManager* cm, ECS::EntityManager* em = nullptr, Core::LUIDRegistry* lr = nullptr, ScriptingEngine* se = nullptr)
            : componentManager(cm), entityManager(em), luidRegistry(lr), scriptingEngine(se) {
            // PhysicsManager is accessed statically, no instance needed
        }

        ~ScriptContext() = default;
    };

} // namespace Scripting
} // namespace NE
