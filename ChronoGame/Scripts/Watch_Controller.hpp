#pragma once
#include "EngineAPI.hpp"

/*
* RMB TO REMOVE THE COMMENTS AND SWAP IN THE APPROPRIATE UI / EVENT SYSTEM CALLS
* Watch_Controller - Manages a regenerating resource with activation system
* Resource regenerates after 1 second of non-usage
* Activation requires >30% resource and consumes at steady rate
* Sends events to Event Bus on state changes
*/

class Watch_Controller : public IScript {
public:
    Watch_Controller() {
        SCRIPT_FIELD(maxResource, Float);
        SCRIPT_FIELD(resourceRegenRate, Float);
        SCRIPT_FIELD(resourceConsumeRate, Float);
        SCRIPT_FIELD(activationThreshold, Float);
        SCRIPT_FIELD(minDeactivationTime, Float);
        // UI REF
    }
    ~Watch_Controller() override = default;

    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}

    void Start() override {
        currentResource = maxResource;
        isActive = false;
        timeSinceLastUse = 0.0f;
        timeSinceActivation = 0.0f;

        LOG_DEBUG("Watch initialized with max resource: " + std::to_string(maxResource));
        // Put any listeners on Event Bus here if needed
    }

    void Update(double deltaTime) override {

        // Trigger Code

        if (Input::WasKeyPressed('1')) {
            Events::Send("ChronoActivated");
            LOG_INFO("Puzzle_Sinkhole_TestTrigger: ChronoActivated sent");
            LOG_INFO("You are now in the past !");
        }

        if (Input::WasKeyPressed('2')) {
            Events::Send("ChronoDeactivated");
            LOG_INFO("Puzzle_Sinkhole_TestTrigger: ChronoDeactivated sent");
            LOG_INFO("You are now in the present !");
        }

        return;

        float dt = static_cast<float>(deltaTime);

        // === Handle Input ===
        if (Input::WasKeyPressed('E')) {
            HandleActivationInput();
        }

        // === Resource Management ===
        if (isActive) {
            // Consume resource while active
            currentResource -= resourceConsumeRate * dt;
            timeSinceActivation += dt;
            timeSinceLastUse = 0.0f; // Reset regen timer

            // Auto-deactivate if depleted
            if (currentResource <= 0.0f) {
                currentResource = 0.0f;
                DeactivateWatch();
            }
        }
        else {
            // Regeneration logic
            timeSinceLastUse += dt;

            // Start regenerating after 1 second of non-usage
            if (timeSinceLastUse >= 1.0f && currentResource < maxResource) {
                currentResource += resourceRegenRate * dt;

                // Cap at max
                if (currentResource > maxResource) {
                    currentResource = maxResource;
                }
            }
        }

        // === Update UI (Debug) ===
        UpdateResourceUI(deltaTime);
    }

    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Watch_Controller"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    void HandleActivationInput() {
        if (isActive) {
            // Try to deactivate
            if (timeSinceActivation >= minDeactivationTime) {
                DeactivateWatch();
            }
            else {
                LOG_DEBUG("Cannot deactivate yet! Time since activation: " +
                    std::to_string(timeSinceActivation) + "s (need " +
                    std::to_string(minDeactivationTime) + "s)");
            }
        }
        else {
            // Try to activate
            float resourcePercent = currentResource / maxResource;
            if (resourcePercent > activationThreshold) {
                ActivateWatch();
            }
            else {
                LOG_DEBUG("Not enough resource to activate! Current: " +
                    std::to_string(resourcePercent * 100.0f) + "% (need >" +
                    std::to_string(activationThreshold * 100.0f) + "%)");
            }
        }
    }

    void ActivateWatch() {
        isActive = true;
        timeSinceActivation = 0.0f;

        // Event Bus placeholder - send activation event
        LOG_DEBUG("=== EVENT BUS: WATCH ACTIVATED ===");
        LOG_DEBUG("Resource at activation: " + std::to_string(currentResource) +
            " / " + std::to_string(maxResource));
    }

    void DeactivateWatch() {
        isActive = false;
        timeSinceActivation = 0.0f;

        // Event Bus placeholder - send deactivation event
        LOG_DEBUG("=== EVENT BUS: WATCH DEACTIVATED ===");
        LOG_DEBUG("Resource at deactivation: " + std::to_string(currentResource) +
            " / " + std::to_string(maxResource));
    }

    void UpdateResourceUI(double dt) {
        // Calculate fill amount for UI ring (0.0 to 1.0)
        float fillAmount = currentResource / maxResource;
        float percentDisplay = fillAmount * 100.0f;

        // UI System placeholder - update ring fill
        // This would normally update a UI component, but we'll just log periodically
        static float lastLogTime = 0.0f;
        static float logInterval = 1.0f; // Log every 1 second
        lastLogTime += static_cast<float>(dt);

        if (lastLogTime >= logInterval) {
            LOG_DEBUG("UI UPDATE: Resource Ring Fill = " +
                std::to_string(percentDisplay) + "% (" +
                std::to_string(currentResource) + " / " +
                std::to_string(maxResource) + ") | State: " +
                std::string(isActive ? "ACTIVE" : "INACTIVE"));
            lastLogTime = 0.0f;
        }
    }

    // === State Variables ===
    bool isActive;
    float currentResource;
    float timeSinceLastUse;
    float timeSinceActivation;

    // === Configuration Fields (visible in inspector) ===
    float maxResource = 100.0f;              // Maximum resource capacity
    float resourceRegenRate = 20.0f;         // Resource per second regeneration
    float resourceConsumeRate = 50.0f;       // Resource per second consumption
    float activationThreshold = 0.3f;        // 30% minimum to activate
    float minDeactivationTime = 0.5f;        // 0.5 seconds minimum active time
};