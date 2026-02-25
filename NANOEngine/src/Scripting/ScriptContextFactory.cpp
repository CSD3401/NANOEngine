#include "pch.h"
/**
 * @file ScriptContextFactory.cpp
 * @brief Implementation of ScriptContext factory functions
 */

#include "ScriptContextFactory.hpp"
#include "ScriptContext.hpp"
#include "ScriptingEngine.hpp"

namespace NE {
namespace Scripting {

    ScriptContext* CreateScriptContext(ECS::ComponentManager* componentManager, ECS::EntityManager* entityManager, Core::LUIDRegistry* luidRegistry, ScriptingEngine* scriptingEngine) {
        return new ScriptContext(componentManager, entityManager, luidRegistry, scriptingEngine);
    }

    void DestroyScriptContext(ScriptContext* context) {
        delete context;
    }

} // namespace Scripting
} // namespace NE
