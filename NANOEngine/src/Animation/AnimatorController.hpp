#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace NE::Animation {

    enum class ParamType { Bool, Float, Int, Trigger };
    enum class CondOp { If, IfNot, Greater, Less, Equals, NotEquals };

    struct Parameter {
        std::string name;
        ParamType   type = ParamType::Bool;
        bool  b = false;
        float f = 0.0f;
        int   i = 0;
    };

    struct Condition {
        std::string param;
        CondOp op = CondOp::If;
        bool  b = false;
        float f = 0.0f;
        int   i = 0;
    };

    struct Transition {
        uint32_t toState = 0;
        std::vector<Condition> conditions;
        bool  hasExitTime = false;
        float exitTimeNormalized = 0.0f;
        float duration = 0.2f;
        bool  canTransitionToSelf = false;
    };

    struct State {
        std::string name;
        std::string clipId;
        float speed = 1.0f;
        std::vector<Transition> transitions;
    };

    struct AnimatorController {
        std::string name;
        uint32_t defaultState = 0;
        std::vector<Parameter> parameters;
        std::vector<State> states;
    };

    struct AnimatorInstance {
        uint32_t currentState = 0;
        float timeInState = 0.0f;

        bool inTransition = false;
        uint32_t nextState = 0;
        float transitionElapsed = 0.0f;
        float transitionDuration = 0.0f;

        std::unordered_map<std::string, bool> triggers; // consumed after use
    };

} // namespace NE::Animation
