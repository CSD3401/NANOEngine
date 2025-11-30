#include <iostream>
#include "EngineAPI.hpp"

/**
 * Test script to demonstrate the new built-in field system and entity activation.
 * This script shows how easy it is to expose fields to the editor and control entity active states.
 *
 * DIRTY FLAG SYSTEM:
 * - Changes made during Play Mode do NOT mark components dirty (they reset on Stop)
 * - Changes made during Editor Mode DO mark components dirty (they are serialized)
 * - Use MarkComponentDirty<T>() manually if needed when writing Editor Scripts
 *
 * ENTITY ACTIVATION DEMO:
 * - Press 'R' to trigger respawn (deactivate then reactivate after delay)
 * - Use Inspector "isActive" checkbox to toggle entity active state manually
 * - Use Inspector script "Enabled" checkbox to enable/disable this script
 */

class TestScript : public IScript {
public:
	TestScript() {
		// Register all our fields using the simple macros
		SCRIPT_FIELD(rotationSpeed, Float);
		SCRIPT_FIELD(bounceHeight, Float);
		SCRIPT_FIELD(color, Vec3);
		SCRIPT_FIELD(particleCount, Int);
		SCRIPT_FIELD(objectName, String);
		SCRIPT_FIELD(respawnDelay, Float);
		SCRIPT_FIELD(enableAutoRespawn, Bool);

		// Register LayerMask field for testing
		SCRIPT_FIELD_LAYERMASK(collisionLayers);

		//std::cout << "[TestScript] Created with fields registered" << std::endl;
	}

    void Initialize(Entity entity) override {
		//std::cout << "[TestScript] Initialized for entity " << entity << std::endl;
		//std::cout << "  Controls:" << std::endl;
		//std::cout << "  - Press 'R' to trigger respawn (deactivate then reactivate)" << std::endl;
		//std::cout << "  - Use Inspector 'isActive' checkbox to toggle entity on/off" << std::endl;
		//std::cout << "  - Use Inspector 'Enabled' checkbox to enable/disable this script" << std::endl;

		//// Test LayerMask functionality
		//std::cout << "[TestScript] LayerMask Test:" << std::endl;
		//std::cout << "  Collision Layers Mask: " << collisionLayers.GetMask() << std::endl;
		//std::cout << "  Contains Layer 0 (Default): " << (collisionLayers.Contains(0) ? "Yes" : "No") << std::endl;
		//std::cout << "  Contains Layer 2 (Player): " << (collisionLayers.Contains(2) ? "Yes" : "No") << std::endl;

		// Programmatically set some layers for testing
		collisionLayers.Add(0);  // Add Default layer
		collisionLayers.Add(2);  // Add Player layer
		//std::cout << "  After adding layers 0 and 2, mask = " << collisionLayers.GetMask() << std::endl;
	}

	void Update(double deltaTime) override {

		// Handle respawn countdown
		if (m_isRespawning) {
			m_respawnTimer -= static_cast<float>(deltaTime);
			
			if (m_respawnTimer <= 0.0f) {
				// Respawn complete - reactivate entity
				SetActive(GetEntity(), true);
				m_isRespawning = false;
			/*	std::cout << "[TestScript] Entity '" << objectName << "' respawned!" << std::endl;
				std::cout << "  - Scripts: ENABLED" << std::endl;
				std::cout << "  - Rendering: ENABLED" << std::endl;
				std::cout << "  - Physics: ENABLED" << std::endl;*/
			}
			return; // Don't do normal update logic while respawning
		}

		// === Input Controls ===
		
		// Trigger respawn sequence with 'R' key
		if (Input::WasKeyPressed('R')) {
			TriggerRespawn();
		}

		// === Visual Demo (Bobbing Motion) ===
		// 
		static float totalTime = 0.0f;
		totalTime += static_cast<float>(deltaTime);

		// Simple bobbing motion using bounce height using SDK methods
		Vec3 currentPos = GetPosition();
		currentPos.y = std::sin(totalTime * 2.0f) * bounceHeight;
		SetPosition(currentPos);

		// === Auto-Respawn Demo ===
		if (enableAutoRespawn) {
			m_autoRespawnTimer += static_cast<float>(deltaTime);
			
			if (m_autoRespawnTimer >= 5.0f) { // Auto-respawn every 5 seconds
				TriggerRespawn();
				m_autoRespawnTimer = 0.0f;
			}
		}
	}

	void OnEnable() override {
		std::cout << "[TestScript] Entity '" << objectName << "' ENABLED (OnEnable called)" << std::endl;
		std::cout << "  - This entity is now fully active!" << std::endl;
	}

	void OnDisable() override {
		std::cout << "[TestScript] Entity '" << objectName << "' DISABLED (OnDisable called)" << std::endl;
		std::cout << "  - Scripts stopped, rendering disabled, physics disabled" << std::endl;
	}

	void OnDestroy() override {
		std::cout << "[TestScript] Entity '" << objectName << "' destroyed" << std::endl;
	}

	const char* GetTypeName() const override {
		return "TestScript";
	}

	// Event handlers (required by interface)
	void OnCollisionEnter(Entity other) override {
		std::cout << "[TestScript] Collision with entity " << other << std::endl;
	}

	void OnCollisionExit(Entity other) override {}
	void OnTriggerEnter(Entity other) override {}
	void OnTriggerExit(Entity other) override {}

private:
	void TriggerRespawn() {
		if (m_isRespawning) {
			std::cout << "[TestScript] Already respawning!" << std::endl;
			return;
		}

		std::cout << "[TestScript] Entity '" << objectName << "' starting respawn sequence..." << std::endl;
		std::cout << "  Will respawn in " << respawnDelay << " seconds" << std::endl;
		
		// Deactivate entity
		SetActive(GetEntity(),false);
		
		// Start respawn countdown
		m_isRespawning = true;
		m_respawnTimer = respawnDelay;
	}

	// === Exposed Fields ===
	// These will automatically appear in the editor inspector
	float rotationSpeed = 90.0f;  // degrees per second
	float bounceHeight = 1.0f;    // units
	Vec3 color{1.0f, 0.0f, 0.0f};  // red by default
	int particleCount = 50;
	std::string objectName = "TestObject";
	float respawnDelay = 2.0f;    // seconds before respawn
	bool enableAutoRespawn = false; // Auto-respawn every 5 seconds

	// LayerMask field for testing (Unity-style multi-select layer picker)
	LayerMask collisionLayers;

	// === Internal State (Not exposed to editor) ===
	float m_totalTime = 0.0f;
	bool m_isRespawning = false;
	float m_respawnTimer = 0.0f;
	float m_autoRespawnTimer = 0.0f;
};