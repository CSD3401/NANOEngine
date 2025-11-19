/**
 * @file IScript_Compat.hpp
 * @brief Backward compatibility layer for old IScript.hpp includes
 *
 * This header provides backward compatibility for existing code that includes
 * the old "IScript.hpp" directly. It maps the old namespace structure to the
 * new clean SDK API.
 *
 * LEGACY USAGE (still works):
 * #include "Scripting/IScript.hpp"
 * class MyScript : public IScript { ... }
 *
 * NEW RECOMMENDED USAGE:
 * #include <ScriptSDK/ScriptAPI.h>
 * class MyScript : public NE::Scripting::IScript { ... }
 */

#pragma once

// Include the new clean SDK
#include "../../include/ScriptSDK/ScriptAPI.h"
#include "../../include/ScriptSDK/ScriptMacros.h"

// Import the old NE::Math::Vec3 type (engine internal)
#include "../Math/Vec3.hpp"

// Provide global namespace aliases for backward compatibility
using IScript = NE::Scripting::IScript;
using IScriptRegistrar = NE::Scripting::IScriptRegistrar;

// Namespace aliases for component types (used in existing scripts)
namespace NE {
    namespace ECS {
        using Entity = Scripting::Entity;
    }
}

// Legacy macros (already defined in ScriptMacros.h, but ensure they're available)
// These are already included from ScriptMacros.h

/**
 * MIGRATION NOTE:
 *
 * This compatibility layer allows existing scripts to compile without changes.
 * However, new scripts should use the clean SDK headers directly:
 *
 * OLD WAY:
 * ```cpp
 * #include "Scripting/IScript.hpp"
 * class MyScript : public IScript {
 *     NE::Math::Vec3 position;  // Engine internal type
 * };
 * ```
 *
 * NEW WAY:
 * ```cpp
 * #include <ScriptSDK/ScriptAPI.h>
 * class MyScript : public NE::Scripting::IScript {
 *     NE::Scripting::Vec3 position;  // SDK type (no engine dependency)
 * };
 * ```
 */
