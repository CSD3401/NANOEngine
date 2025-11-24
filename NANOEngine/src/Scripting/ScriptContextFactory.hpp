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

namespace NE {
namespace Scripting {

    // Forward declaration
    class ScriptContext;

    /**
     * Create a ScriptContext from a ComponentManager and EntityManager.
     * This is used internally by the engine to maintain backward compatibility.
     *
     * @param componentManager The component manager to wrap
     * @param entityManager The entity manager for LUID resolution
     * @return Pointer to a new ScriptContext (caller owns memory)
     */
    ScriptContext* CreateScriptContext(ECS::ComponentManager* componentManager, ECS::EntityManager* entityManager = nullptr);

    /**
     * Destroy a ScriptContext.
     * @param context The context to destroy
     */
    void DestroyScriptContext(ScriptContext* context);

    /**
     * Helper function to link a script to the engine using ComponentManager and EntityManager.
     * This provides backward compatibility with the old LinkToEngine API.
     *
     * @param script The script to link
     * @param componentManager The component manager
     * @param entityManager The entity manager for LUID resolution
     */
    inline void LinkScriptToEngine(IScript* script, ECS::ComponentManager* componentManager, ECS::EntityManager* entityManager = nullptr) {
        ScriptContext* context = CreateScriptContext(componentManager, entityManager);
        script->_LinkToEngine(context);
    }

} // namespace Scripting
} // namespace NE
