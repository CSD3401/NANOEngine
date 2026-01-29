#pragma once
#include "EngineAPI.hpp"

/**
 * @class CameraController
 * @brief Example script showing how to control camera properties from scripts
 *
 * This script demonstrates all the new camera operations:
 * - Changing FOV (Field of View)
 * - Adjusting aspect ratio
 * - Modifying near and far clipping planes
 * - Setting main camera flag
 * - Toggling camera active state
 */
class CameraController : public IScript {
public:
	CameraController() = default;
	~CameraController() override = default;

	void Initialize(Entity entity) override {
		// Register fields for editor exposure
		RegisterFloatField("targetFOV", &targetFOV);
		RegisterFloatField("fovChangeSpeed", &fovChangeSpeed);
		RegisterFloatField("minFOV", &minFOV);
		RegisterFloatField("maxFOV", &maxFOV);
		RegisterBoolField("allowFOVChange", &allowFOVChange);

		SCRIPT_FIELD(targetFOV, Float);
		SCRIPT_FIELD(fovChangeSpeed, Float);
		SCRIPT_FIELD(minFOV, Float);
		SCRIPT_FIELD(maxFOV, Float);
		SCRIPT_FIELD(allowFOVChange, Bool);
	}

	void Start() override {
		// Check if this entity has a camera component
		if (!HasCamera()) {
			LOG_ERROR("CameraController: Entity " << GetEntity() << " does not have a Camera component!");
			SetEnabled(false);
			return;
		}

		// Log initial camera properties
		LOG_INFO("=== Camera Controller Started ===");
		LOG_INFO("Initial FOV: " << GetCameraFOV());
		LOG_INFO("Aspect Ratio: " << GetCameraAspectRatio());
		LOG_INFO("Near Plane: " << GetCameraNearPlane());
		LOG_INFO("Far Plane: " << GetCameraFarPlane());
		LOG_INFO("Is Main Camera: " << (IsCameraMain() ? "Yes" : "No"));
		LOG_INFO("Is Active: " << (IsCameraActive() ? "Yes" : "No"));
		LOG_INFO("===============================");
	}

	void Update(double deltaTime) override {
		if (allowFOVChange) {
			// Clamp targetFOV to current min/max bounds
			targetFOV = std::max(minFOV, std::min(maxFOV, targetFOV));

			float currentFOV = GetCameraFOV();
			if (std::abs(currentFOV - targetFOV) > 0.1f) {
				float newFOV = currentFOV + (targetFOV - currentFOV) * fovChangeSpeed * static_cast<float>(deltaTime);
				newFOV = std::max(minFOV, std::min(maxFOV, newFOV));
				SetCameraFOV(newFOV);
			}
		}
	}

	void OnDestroy() override {
		LOG_INFO("CameraController destroyed");
	}

	const char* GetTypeName() const override {
		return "CameraController";
	}

	// Event handlers (required by interface)
	void OnCollisionEnter(Entity other) override {}
	void OnCollisionExit(Entity other) override {}
	void OnTriggerEnter(Entity other) override {}
	void OnTriggerExit(Entity other) override {}

private:
	// === Exposed Fields ===
	float targetFOV = 60.0f;   // Target FOV to smoothly transition to
	float fovChangeSpeed = 2.0f;       // Speed of FOV transition
	float minFOV = 30.0f;          // Minimum FOV (zoomed in)
	float maxFOV = 120.0f; // Maximum FOV (zoomed out)
	bool allowFOVChange = true;        // Enable/disable smooth FOV changes

	// Internal variables
	float zoomSpeed = 30.0f;           // Speed for manual zoom with Q/E keys
};