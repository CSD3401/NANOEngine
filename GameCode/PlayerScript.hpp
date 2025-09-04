#pragma once

// This interface comes from the Engine project.
#include "src/Scripting/IScript.hpp"

// The PlayerScript class inherits from the engine's IScript interface.
// This is the contract that allows the engine to call its methods.
//class PlayerScript : public NE::Scripting::IScript{
//public:
//    // We override the virtual functions from the IScript interface.
//    void OnCreate() override;
//    void OnUpdate(double deltaTime) override;
//    void OnDestroy() override;
//
//private:
//    // Scripts can have their own member variables to hold state.
//    float m_Speed = 10.0f;
//};
