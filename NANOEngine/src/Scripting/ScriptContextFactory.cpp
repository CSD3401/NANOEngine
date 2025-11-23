/**
 * @file ScriptContextFactory.cpp
 * @brief Implementation of ScriptContext factory functions
 */

#include "ScriptContextFactory.hpp"
#include "ScriptContext.hpp"

namespace NE {
namespace Scripting {

    ScriptContext* CreateScriptContext(ECS::ComponentManager* componentManager, ECS::EntityManager* entityManager) {
        return new ScriptContext(componentManager, entityManager);
    }

    void DestroyScriptContext(ScriptContext* context) {
        delete context;
    }

} // namespace Scripting
} // namespace NE
