// This header is from your 'Engine' project. Visual Studio can find it because
// we set up the "Additional Include Directories".
#include "src/Scripting/ScriptingEngine.hpp"
#include "pch.h"

// We also need to include the header for every script we want to register.
//#include "PlayerScript.h"

// extern "C" is crucial. It tells the C++ compiler not to "mangle" the
// function name, so our engine can find it by its exact name "RegisterEngineScripts".
extern "C" {
    // __declspec(dllexport) is the Microsoft-specific keyword that makes this
    // function visible and usable outside of this DLL. This is the magic
    // that allows your engine to call it.
    //__declspec(dllexport)
    //    void RegisterEngineScripts(NE::Scripting::ScriptingEngine* engine) {
    //    // For every script you create in this project, you must add a line here
    //    // to tell the engine about it.

    //    // The first argument is the "name" you will use in the editor to refer
    //    // to the script. The second is a lambda function that creates a new
    //    // instance of the script object.
    //    engine->RegisterScript("PlayerScript", []() -> IScript* { return new PlayerScript(); });

    //    // If you created an "EnemyAI" script, you would add it like this:
    //    // engine->RegisterScript("EnemyAI", []() -> IScript* { return new EnemyAI(); });
    //}
}