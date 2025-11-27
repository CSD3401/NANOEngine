#pragma once
#include "../ScriptBase.hpp"

/**
 * MaterialSwapperExample - Demonstrates vector of materials in inspector
 *
 * This script shows how to:
 * - Register a vector of material UUIDs that appears in the inspector
 * - Swap materials at runtime using key presses
 * - Store and manipulate material references
 */
class MaterialSwapperExample : public ScriptBase<MaterialSwapperExample> {
public:
	MaterialSwapperExample() {
		// Register fields for inspector
		REGISTER_FIELD(currentMaterialIndex);
		REGISTER_VECTOR(materialUUIDs);  // Vector of material UUIDs - editable in inspector!

		// Pre-fill with some example material UUIDs (these are just examples)
		materialUUIDs = {
				   "material-red-uuid-here",
		  "material-blue-uuid-here",
			 "material-green-uuid-here"
		};
	}

	~MaterialSwapperExample() override = default;

	void Awake() override {
		// Called when script is created
	}

	void Initialize(Entity entity) override {
		// Apply the first material if available
		if (!materialUUIDs.empty() && currentMaterialIndex < materialUUIDs.size()) {
			ApplyMaterial(materialUUIDs[currentMaterialIndex]);
		}
	}

	void Start() override {
		LOG_INFO("MaterialSwapperExample: Press 1-9 to swap materials, or N/M to cycle");
	}

	void Update(double deltaTime) override {
		// Press 'N' to go to next material
		if (Input::WasKeyPressed('N')) {
			NextMaterial();
		}

		// Press 'M' to go to previous material
		if (Input::WasKeyPressed('M')) {
			PreviousMaterial();
		}

		// Press number keys 1-9 to jump to specific material
		for (int i = 1; i <= 9; i++) {
			if (Input::WasKeyPressed('0' + i)) {
				SetMaterialIndex(i - 1);
				break;
			}
		}
	}

	void OnDestroy() override {
		// Cleanup if needed
	}

	void OnValidate() override {
		// Called when values change in inspector
		// Clamp current index to valid range
		if (currentMaterialIndex >= static_cast<int>(materialUUIDs.size()) && !materialUUIDs.empty()) {
			currentMaterialIndex = static_cast<int>(materialUUIDs.size()) - 1;
		}
	}

	const char* GetTypeName() const override {
		return "MaterialSwapperExample";
	}

	// === Collision Callbacks ===
	void OnCollisionEnter(Entity other) override {}
	void OnCollisionExit(Entity other) override {}
	void OnTriggerEnter(Entity other) override {}
	void OnTriggerExit(Entity other) override {}

private:
	// Editable in inspector
	int currentMaterialIndex = 0;
	std::vector<std::string> materialUUIDs;  // Vector of material UUIDs - shows up in inspector!

	void NextMaterial() {
		if (materialUUIDs.empty()) return;

		currentMaterialIndex = (currentMaterialIndex + 1) % materialUUIDs.size();
		ApplyMaterial(materialUUIDs[currentMaterialIndex]);

		LOG_INFO("Switched to material " << currentMaterialIndex + 1 << " of " << materialUUIDs.size());
	}

	void PreviousMaterial() {
		if (materialUUIDs.empty()) return;

		currentMaterialIndex--;
		if (currentMaterialIndex < 0) {
			currentMaterialIndex = static_cast<int>(materialUUIDs.size()) - 1;
		}

		ApplyMaterial(materialUUIDs[currentMaterialIndex]);

		LOG_INFO("Switched to material " << currentMaterialIndex + 1 << " of " << materialUUIDs.size());
	}

	void SetMaterialIndex(int index) {
		if (materialUUIDs.empty()) return;
		if (index < 0 || index >= static_cast<int>(materialUUIDs.size())) {
			LOG_WARNING("Material index out of range: " << index);
			return;
		}

		currentMaterialIndex = index;
		ApplyMaterial(materialUUIDs[currentMaterialIndex]);

		LOG_INFO("Switched to material " << currentMaterialIndex + 1 << " of " << materialUUIDs.size());
	}

	void ApplyMaterial(const std::string& materialUUID) {
		if (materialUUID.empty()) {
			LOG_WARNING("Cannot apply empty material UUID");
			return;
		}

		// Apply the material to this entity's renderer
		NE::Renderer::Command::AssignMaterial(GetEntity(), materialUUID);
	}
};