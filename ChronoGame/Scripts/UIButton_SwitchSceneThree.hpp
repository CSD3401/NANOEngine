#pragma once
#include "EngineAPI.hpp"

class UIButton_SwitchSceneThree : public IScript {
public:
    UIButton_SwitchSceneThree() {
       // SCRIPT_FIELD(scenePath, String);
    }

    ~UIButton_SwitchSceneThree() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {
        m_Button = entity;
        //if (scenePath.empty()) scenePath = "23817f87-176c-4c6d-84a9-1999ac689ce9";
    }
    void Start() override {
       //scenePath = "23817f87-176c-4c6d-84a9-1999ac689ce9";
        scenePath = "e17cc794-74d9-40ee-9c9e-efaa829ab09a";
    }

    void Update(double /*dt*/) override {
        if (m_sent) return;

        if (NE::Scripting::WasButtonClicked(m_Button) &&
            NE::Scripting::IsButtonInteractable(m_Button)) {
            m_sent = true;
            NE::Scripting::SwitchScene(scenePath); // now queues safely
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UIButton_SwitchScene"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    Entity m_Button{};
    std::string scenePath{}; /*= "23817f87-176c-4c6d-84a9-1999ac689ce9";*/
    bool m_sent = false;
};
