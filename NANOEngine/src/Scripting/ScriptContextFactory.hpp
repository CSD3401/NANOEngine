/**
 * @file ScriptContextFactory.hpp
 * @brief Internal factory for creating ScriptContext instances
 *
 * This is an INTERNAL ENGINE HEADER - not part of the public SDK.
 * It provides a bridge between the old ComponentManager-based API
 * and the new ScriptContext-based API.
 */

#pragma once

#include "../../include/ScriptSDK/ScriptAPI.h"
#include "../ECS/Core/ComponentManager.hpp"
#include "../ECS/Core/EntityManager.hpp"

namespace NE::Core {
    class LUIDRegistry;
}

namespace NE {
namespace Scripting {

    // Forward declarations
    class ScriptContext;
    class ScriptingEngine;

    /**
     * Create a ScriptContext from a ComponentManager, EntityManager, LUIDRegistry, and ScriptingEngine.
     * This is used internally by the engine to maintain backward compatibility.
     *
     * @param componentManager The component manager to wrap
     * @param entityManager The entity manager for LUID resolution
     * @param luidRegistry The LUID registry for component lookups
     * @param scriptingEngine The scripting engine for script-to-script communication
     * @return Pointer to a new ScriptContext (caller owns memory)
     */
    ScriptContext* CreateScriptContext(ECS::ComponentManager* componentManager, ECS::EntityManager* entityManager = nullptr, Core::LUIDRegistry* luidRegistry = nullptr, ScriptingEngine* scriptingEngine = nullptr);

    /**
     * Destroy a ScriptContext.
     * @param context The context to destroy
     */
    void DestroyScriptContext(ScriptContext* context);

    /**
     * Helper function to link a script to the engine using ComponentManager, EntityManager, LUIDRegistry, and ScriptingEngine.
     * This provides backward compatibility with the old LinkToEngine API.
     *
     * @param script The script to link
     * @param componentManager The component manager
     * @param entityManager The entity manager for LUID resolution
     * @param luidRegistry The LUID registry for component lookups
     * @param scriptingEngine The scripting engine for script-to-script communication
     */
    inline void LinkScriptToEngine(IScript* script, ECS::ComponentManager* componentManager, ECS::EntityManager* entityManager = nullptr, Core::LUIDRegistry* luidRegistry = nullptr, ScriptingEngine* scriptingEngine = nullptr) {
        ScriptContext* context = CreateScriptContext(componentManager, entityManager, luidRegistry, scriptingEngine);
        script->_LinkToEngine(context);
    }

} // namespace Scripting
} // namespace NE
