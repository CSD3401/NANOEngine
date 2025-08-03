#include "EngineState.hpp"

namespace NANOEngine {
    EngineState g_EngineState = EngineState::Edit;

    void SetEngineState(EngineState state) { g_EngineState = state; }
    EngineState GetEngineState() { return g_EngineState; }
}