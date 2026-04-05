#pragma once
#include "EngineAPI.hpp"

class UIButton_SwitchSceneTwo : public IScript {
public:
    UIButton_SwitchSceneTwo() {
        SCRIPT_FIELD(switchDelaySeconds, Float);
    }

    ~UIButton_SwitchSceneTwo() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {
        m_Button = entity;
        //if (scenePath.empty()) scenePath = "23817f87-176c-4c6d-84a9-1999ac689ce9";
    }
    void Start() override {
        //scenePath = "23817f87-176c-4c6d-84a9-1999ac689ce9";
        scenePath = "7271eb07-beef-4306-b5d6-1b6ef60418d3";
    }

    void Update(double deltaTime) override {
        if (m_sent) return;

        if (m_pendingDelay) {
            m_delayElapsed += static_cast<float>(deltaTime);
            if (m_delayElapsed >= switchDelaySeconds) {
                m_sent = true;
                NE::Scripting::SwitchScene(scenePath);
            }
            return;
        }

        if (NE::Scripting::WasButtonClicked(m_Button) &&
            NE::Scripting::IsButtonInteractable(m_Button)) {
            if (switchDelaySeconds <= 0.0f) {
                m_sent = true;
                NE::Scripting::SwitchScene(scenePath);
            }
            else {
                m_pendingDelay = true;
                m_delayElapsed = 0.0f;
            }
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UIButton_SwitchSceneTwo"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    Entity m_Button{};
    std::string scenePath{}; /*= "23817f87-176c-4c6d-84a9-1999ac689ce9";*/
    float switchDelaySeconds = 0.0f;
    bool m_sent = false;
    bool m_pendingDelay = false;
    float m_delayElapsed = 0.0f;
};
