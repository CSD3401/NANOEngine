#pragma once
#include <memory>
#include <algorithm>
#include <numbers>
#include "EngineAPI.hpp"
/*
* === WORK IN PROGRESS ===
* By Chan Kuan Fu Ryan (c.kuanfuryan)
* Manager_ is a static instance which provides centralised storage
* for frequently used game data and methods.
*/

class Misc_Manager : public IScript {
public:
    Misc_Manager() {
		SCRIPT_FIELD(highlightMaterial, MaterialRef);
    }
    ~Misc_Manager() override = default;

    // === Custom Methods ===
    MaterialRef GetHighlightMaterial() const {
        return highlightMaterial;
	}
    float SnappyLerp(
        float current, 
        float target, 
        float snappiness,
        float deltaTime)
    {
        float factor = 1.0f - std::exp(-snappiness * deltaTime);
        return current + (target - current) * factor;
    }
    float DegreesToRadians(float degrees)
    {
        return degrees * (std::numbers::pi_v<float> / 180.0f);
    }

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override {}
    void Update(double deltaTime) override {}
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Misc_Manager"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    MaterialRef highlightMaterial;
};