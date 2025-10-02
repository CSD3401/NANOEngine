#pragma once

#include <string>
#include <functional>
#include "IScript.hpp"

// Forward declaration
class IScript;

namespace NE::Scripting {

    /**
     * Interface for registering scripts with the scripting engine.
     * This provides a clean separation between the engine and game-specific scripts.
     */
    class IScriptRegistrar {
    public:
        virtual ~IScriptRegistrar() = default;

        /**
         * Register a script type with the scripting system.
         * @param name The name used to identify this script type
         * @param factory Function that creates a new instance of the script
         */
        virtual void RegisterScript(const std::string& name, std::function<IScript* ()> factory) = 0;

        /**
         * Optional: Check if a script type is already registered
         * @param name The script name to check
         * @return true if the script is registered, false otherwise
         */
        virtual bool IsScriptRegistered(const std::string& name) const = 0;

        /**
         * Optional: Get the number of registered scripts
         * @return Number of registered script types
         */
        virtual size_t GetRegisteredScriptCount() const = 0;
    };

}