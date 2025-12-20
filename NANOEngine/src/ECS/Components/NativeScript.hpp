#pragma once
#include "../../Core/Reflection.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace NE::ECS::Component {
    /**
     * @struct NativeScript
     * @brief Pure data component for script attachment
     *
     * Following ECS methodology: this component contains ONLY data.
     * All runtime instances are managed by ScriptEngine.
     * ScriptSystem updates this data and delegates instance operations to ScriptEngine.
     *
     * To assign a script to an entity, use:
     *   - From Editor/UI: NE::ECS::Command::SetEntityScript(entity, scriptName)
     *   - From code: Set ScriptName directly and mark IsDirty = true
     */
    struct NativeScript {
        // The name of the script class, e.g., "PlayerScript"
        std::string ScriptName;

        // Serialized field values (field name -> string value)
        // Populated when saving scene, restored when loading scene
        std::unordered_map<std::string, std::string> SerializedFields;

        // Track which fields contain entity references (need LUID conversion during serialization)
        // Includes: transformref, rigidbodyref, audiosourceref, vector<entity>
        std::unordered_set<std::string> EntityReferenceFields;

        // Dirty flag - set when ScriptName changes or component is modified
        // ScriptSystem checks this to know when to recreate instances
        bool IsDirty = true;

        // LUID for serialization
        uint64_t luid;

        NE_REFLECT_BEGIN(NativeScript)
            NE_REFLECT_FIELD(ScriptName)
            //NE_REFLECT_FIELD(SerializedFields)
            //NE_REFLECT_FIELD(EntityReferenceFields)
        NE_REFLECT_END()
    };
}

