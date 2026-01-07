#pragma once
#include "../../Core/Reflection.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NE::ECS::Component {
    /**
     * @struct NativeScript
     * @brief Pure data component for script attachment
     *
     * Following ECS methodology: this component contains ONLY data.
     * All runtime instances are managed by ScriptEngine.
     * ScriptSystem updates this data and delegates instance operations to ScriptEngine.
     *
     * Supports multiple scripts per entity. Scripts are executed in the order they appear
     * in ScriptNames vector. Field values are stored in a shared pool with keys formatted
     * as "ScriptName.fieldName" to avoid conflicts between scripts.
     *
     * To assign scripts to an entity, use:
     *   - From Editor/UI: Modify ScriptNames vector directly
     *   - From code: Add to ScriptNames vector and mark IsDirty = true
     */
    struct NativeScript {
        // List of script class names, e.g., ["PlayerScript", "HealthScript"]
        // Scripts are executed in the order they appear in this list
        std::vector<std::string> ScriptNames;

        // Serialized field values (field name -> string value)
        // For multi-script support, keys are formatted as "ScriptName.fieldName"
        // Populated when saving scene, restored when loading scene
        std::unordered_map<std::string, std::string> SerializedFields;

        // Track which fields contain entity references (need LUID conversion during serialization)
        // Keys are formatted as "ScriptName.fieldName" for multi-script support
        // Includes: transformref, rigidbodyref, audiosourceref, vector<entity>
        std::unordered_set<std::string> EntityReferenceFields;

        // Dirty flag - set when component is first created or scripts are modified
        // ScriptSystem checks this to know when to create/update instances
        bool IsDirty = true;

        // Internal tracking to detect script type changes
        // Used to determine which scripts need to be added/removed during hot-reload
        std::vector<std::string> _lastScriptNames;

        // LUID for serialization
        uint64_t luid;

        // Helper to get a field key in the format "ScriptName.fieldName"
        static std::string GetFieldKey(const std::string& scriptName, const std::string& fieldName) {
            return scriptName + "." + fieldName;
        }

        // Helper to parse script name from a field key
        static std::string GetScriptNameFromKey(const std::string& fieldKey) {
            size_t dotPos = fieldKey.find('.');
            if (dotPos != std::string::npos) {
                return fieldKey.substr(0, dotPos);
            }
            return fieldKey; // Legacy: no dot means old format
        }

        // Helper to parse field name from a field key
        static std::string GetFieldNameFromKey(const std::string& fieldKey) {
            size_t dotPos = fieldKey.find('.');
            if (dotPos != std::string::npos) {
                return fieldKey.substr(dotPos + 1);
            }
            return fieldKey; // Legacy: no dot means old format
        }

        NE_REFLECT_BEGIN(NativeScript)
            NE_REFLECT_FIELD(ScriptNames)
            // NOTE: SerializedFields and EntityReferenceFields use custom serialization
            // See ReflectionJson.hpp to_json/from_json(NativeScript) for LUID conversion
            // They are intentionally excluded from reflection macro
        NE_REFLECT_END()
    };
}

