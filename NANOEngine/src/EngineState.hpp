#pragma once

namespace NANOEngine {
    enum class EngineState {
        Edit,
        Play,
        Paused
    };

    extern EngineState g_EngineState;

    void SetEngineState(EngineState state);
    EngineState GetEngineState();
}
