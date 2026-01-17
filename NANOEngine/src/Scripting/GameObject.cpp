#include "../../include/ScriptSDK/ScriptAPI.h"
#include "ScriptContext.hpp"
#include "ScriptingEngine.hpp"
#include "../ECS/Components/NativeScript.hpp"
#include "../ECS/Core/ComponentManager.hpp"
#include "../ECS/Core/EntityManager.hpp"
#include "../ECS/Components/EntityMeta.hpp"
#include <string>

namespace NE::Scripting {

    //=========================================================================
    // Static context for Find operations (set when scripts are linked)
    //=========================================================================

    static ScriptContext* g_staticContext = nullptr;

    inline ScriptContext* GetStaticContext() {
        return g_staticContext;
    }

    inline void SetStaticContext(ScriptContext* context) {
        g_staticContext = context;
    }

    inline void ResetStaticContext() {
        g_staticContext = nullptr;
    }

    //=========================================================================
    // GameObject Implementation
    //=========================================================================

    SCRIPT_API GameObject::GameObject(Entity entity, ScriptContext* context)
        : m_entity(entity), m_context(context)
    {
    }

    SCRIPT_API std::string GameObject::GetName() const {
        if (!m_context || !m_context->componentManager || m_entity == INVALID_ENTITY) {
            return "";
        }

        if (!m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(m_entity)) {
            return "";
        }

        return m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(m_entity).name;
    }

    SCRIPT_API void GameObject::SetName(const std::string& name) {
        if (!m_context || !m_context->componentManager || m_entity == INVALID_ENTITY) {
            return;
        }

        if (!m_context->componentManager->HasComponent<ECS::Component::EntityMeta>(m_entity)) {
            return;
        }

        m_context->componentManager->GetComponent<ECS::Component::EntityMeta>(m_entity).name = name;
    }

    SCRIPT_API IScript* GameObject::GetScript(const std::string& scriptName) const {
        EnsureContext();
        if (!m_context || !m_context->scriptingEngine || m_entity == INVALID_ENTITY) {
            return nullptr;
        }

        return m_context->scriptingEngine->GetScriptInstanceByName(m_entity, scriptName);
    }

    SCRIPT_API bool GameObject::HasScript(const std::string& scriptName) const {
        return GetScript(scriptName) != nullptr;
    }

    SCRIPT_API GameObject GameObject::Find(const std::string& name) {
        ScriptContext* ctx = GetStaticContext();
        if (!ctx || !ctx->componentManager) {
            return GameObject();
        }

        // Iterate through all entities to find matching name
        const auto& usedEntities = ctx->entityManager->GetUsedEntities();
        for (Entity entity : usedEntities) {
            if (ctx->componentManager->HasComponent<ECS::Component::EntityMeta>(entity)) {
                const auto& meta = ctx->componentManager->GetComponent<ECS::Component::EntityMeta>(entity);
                if (meta.name == name) {
                    return GameObject(entity, ctx);
                }
            }
        }

        return GameObject(); // Not found
    }

    SCRIPT_API void GameObject::EnsureContext() const {
        // If context is null but we have a valid entity, try to get static context
        if (!m_context && m_entity != INVALID_ENTITY) {
            m_context = GetStaticContext();
        }
    }

    //=========================================================================
    // GameObject Helper Functions (for template implementations)
    //=========================================================================

    SCRIPT_API IScript* GameObject_GetScriptByType(Entity entity, const std::string& typeName) {
        ScriptContext* ctx = GetStaticContext();
        if (!ctx || !ctx->scriptingEngine || !ctx->entityManager || entity == INVALID_ENTITY) {
            return nullptr;
        }

        // Verify entity is still alive (not destroyed) before accessing script instances
        const auto& usedEntities = ctx->entityManager->GetUsedEntities();
        bool entityAlive = false;
        for (Entity e : usedEntities) {
            if (e == entity) {
                entityAlive = true;
                break;
            }
        }
        if (!entityAlive) {
            return nullptr;  // Entity was destroyed
        }

        return ctx->scriptingEngine->GetScriptInstanceByName(entity, typeName);
    }

    SCRIPT_API std::vector<Entity> GameObject_FindAllWithScript(const std::string& typeName) {
        std::vector<Entity> result;

        ScriptContext* ctx = GetStaticContext();
        if (!ctx || !ctx->componentManager || !ctx->entityManager) {
            return result;
        }

        // Iterate through all entities to find matching scripts
        const auto& usedEntities = ctx->entityManager->GetUsedEntities();
        for (Entity entity : usedEntities) {
            // Check if entity has NativeScript component
            if (ctx->componentManager->HasComponent<ECS::Component::NativeScript>(entity)) {
                const auto& nsc = ctx->componentManager->GetComponent<ECS::Component::NativeScript>(entity);

                // Check if any of the script names match
                for (const std::string& scriptName : nsc.ScriptNames) {
                    if (scriptName == typeName) {
                        result.push_back(entity);
                        break; // Found a match, don't add duplicates
                    }
                }
            }
        }

        return result;
    }

    //=========================================================================
    // IScript::gameObject Property Implementation
    //=========================================================================

    SCRIPT_API GameObject IScript::gameObject() const {
        SetStaticContext(m_context);
        return GameObject(m_entity, m_context);
    }

    //=========================================================================
    // Static Context Management (called by Engine)
    //=========================================================================

    SCRIPT_API void GameObject_SetStaticContext(ScriptContext* context) {
        SetStaticContext(context);
    }

    SCRIPT_API void GameObject_ResetStaticContext() {
        ResetStaticContext();
    }

} // namespace NE::Scripting
