#pragma once

namespace NE {
    enum class EngineState {
        Edit,
        Play,
        Paused
    };

    extern EngineState g_EngineState;

    void SetEngineState(EngineState state);
    EngineState GetEngineState();
}
