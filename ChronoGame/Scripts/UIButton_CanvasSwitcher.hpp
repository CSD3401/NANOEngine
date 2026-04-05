#pragma once
#include "EngineAPI.hpp"
#include <ScriptSDK/UI.h>

/**
 * UIButton_CanvasSwitcher
 * -----------------------
 * Generic UI button script for pause submenus.
 * On click, activates all entities in showTargets and deactivates all entities in hideTargets.
 *
 * Typical use:
 * - Settings button: show settings canvas, hide pause main canvas
 * - Controls button: show controls canvas, hide pause main canvas
 * - Back button: show pause main canvas, hide controls/settings canvas
 */
class UIButton_CanvasSwitcher : public IScript {
public:
    UIButton_CanvasSwitcher() {
        RegisterGameObjectRefVectorField("showTargets", &showTargets);
        RegisterGameObjectRefVectorField("hideTargets", &hideTargets);
    }

    ~UIButton_CanvasSwitcher() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override { m_button = entity; }
    void Start() override {}

    void Update(double /*dt*/) override {
        if (m_button == 0) return;
        if (!UI::WasButtonClicked(m_button) || !UI::IsButtonInteractable(m_button))
            return;

        for (auto& ref : hideTargets) {
            if (ref.IsValid())
                SetActive(false, ref.GetEntity());
        }

        for (auto& ref : showTargets) {
            if (ref.IsValid())
                SetActive(true, ref.GetEntity());
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UIButton_CanvasSwitcher"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    Entity m_button = 0;
    std::vector<GameObjectRef> showTargets;
    std::vector<GameObjectRef> hideTargets;
};
