#pragma once
#include "EngineAPI.hpp"

/**
 * UIButton_SwitchToCredits
 * ------------------------
 * Attach to the Credit button on the main menu.
 * On click, switches to the Credit scene.
 */
class UIButton_SwitchToCredits : public IScript {
public:
    UIButton_SwitchToCredits() = default;
    ~UIButton_SwitchToCredits() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override { m_button = entity; }
    void Start() override {}

    void Update(double /*dt*/) override {
        if (m_sent) return;
        if (m_button == 0) return;
        if (!UI::WasButtonClicked(m_button) || !UI::IsButtonInteractable(m_button))
            return;
        m_sent = true;
        NE::Scripting::SwitchScene("7c7bd1dd-30c6-414a-8514-b045d3b54acd");
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UIButton_SwitchToCredits"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    Entity m_button = 0;
    bool m_sent = false;
};
