#pragma once
#include "../../Scripting/IScript.hpp"
#include "../../Core/Reflection.hpp"
#include <string>
#include <functional>
#include <unordered_map>

namespace NE::ECS::Component {
    struct NativeScript {
        std::string ScriptName; // The name of the script class, e.g., "PlayerScript"

        IScript* Instance = nullptr;

        // Function pointers to create and destroy the script instance.
        // These will be provided by the ScriptingEngine.
        std::function<IScript* ()> CreateScript;
        std::function<void(IScript*)> DestroyScript;

        // Serialized field values (field name -> string value)
        // Populated when saving scene, restored when loading scene
        std::unordered_map<std::string, std::string> SerializedFields;

        // Binds the functions from the ScriptingEngine to this component.
        // This is called by the user when adding the component.
        void Bind(const std::string& name) {
            ScriptName = name;
            // The actual function pointers will be looked up and assigned
            // by the ScriptingEngine. We just store the name for now.
        }

        // Unbind is handled automatically when the component is destroyed.
        // The ScriptSystem will call DestroyScript(Instance).

        NE_REFLECT_BEGIN(NativeScript)
            NE_REFLECT_FIELD(ScriptName)
        NE_REFLECT_END()
    };
}

