#pragma once
#include "EngineAPI.hpp"

/*
* UIButton_Quit requests application close when the attached UI button is clicked.
* Attach this script to the main menu Quit button entity.
*/
class UIButton_Quit : public IScript {
public:
    UIButton_Quit() = default;
    ~UIButton_Quit() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {
        m_Button = entity;
    }
    void Start() override {}

    void Update(double /*dt*/) override {
        if (NE::Scripting::WasButtonClicked(m_Button) &&
            NE::Scripting::IsButtonInteractable(m_Button)) {
            NE::Scripting::RequestClose();
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UIButton_Quit"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    Entity m_Button{};
};
