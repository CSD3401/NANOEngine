#pragma once
#include "NANOEngineAPI.hpp"

namespace NE {
    enum class EngineState {
        Edit,
        Play,
        Paused
    };

    extern NANOENGINE_API EngineState g_EngineState;

    NANOENGINE_API void SetEngineState(EngineState state);
    NANOENGINE_API EngineState GetEngineState();
}
