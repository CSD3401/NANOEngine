#pragma once
#include "EngineAPI.hpp"

class UIButton_SwitchSceneThree : public IScript {
public:
    UIButton_SwitchSceneThree() {
        SCRIPT_FIELD(switchDelaySeconds, Float);
    }

    ~UIButton_SwitchSceneThree() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {
        m_Button = entity;
        //if (scenePath.empty()) scenePath = "23817f87-176c-4c6d-84a9-1999ac689ce9";
    }
    void Start() override {
        //scenePath = "23817f87-176c-4c6d-84a9-1999ac689ce9";
        scenePath = "7f6eb653-c6f7-426d-b9ac-9c0bada73cfc";
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

    const char* GetTypeName() const override { return "UIButton_SwitchSceneThree"; }

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
