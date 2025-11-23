#pragma once


#include <vector>
#include "EngineAPI.hpp"

/**
 * Example script demonstrating the new MaterialRef feature
 *
 * This shows how to:
 * 1. Declare single MaterialRef fields
 * 2. Declare vector<MaterialRef> fields
 * 3. Use MaterialRef to assign materials to entities
 */
class ExampleMaterialScript : public IScript {
private:
    // Single material reference
    MaterialRef primaryMaterial;

    // Vector of material references - NOW NATIVE SUPPORT!
    std::vector<MaterialRef> materialList;

    // You can still use strings if you prefer the old way
    std::string legacyMaterialUUID;

public:
    void Initialize(Entity entity) override {
        // Register single material field
        RegisterMaterialRefField("primaryMaterial", &primaryMaterial);

        // Register vector material field
        RegisterMaterialRefVectorField("materialList", &materialList);

        // Legacy string approach still works
        RegisterStringField("legacyMaterialUUID", &legacyMaterialUUID);
    }

    void Update(double deltaTime) override {
        // Example: Assign the primary material to this entity
        if (primaryMaterial.IsValid()) {
            if (Input::WasKeyPressed('X'))
                // Use the helper function that takes MaterialRef directly
                NE::Renderer::Command::AssignMaterial(GetEntity(), primaryMaterial);
        }

        // Example: Use materials from the vector
        // You could cycle through materials, assign to child entities, etc.
        if (!materialList.empty() && materialList[0].IsValid()) {
            if (Input::WasKeyPressed('/'))
            // Assign first material in the list
                NE::Renderer::Command::AssignMaterial(GetEntity(), materialList[0]);
        }

        // Legacy approach with string UUID still works
        /*if (!legacyMaterialUUID.empty()) {
            NE::Renderer::Command::AssignMaterial(GetEntity(), legacyMaterialUUID);
        }*/
    }

    void OnCollisionEnter(NE::Scripting::Entity other) override {}
    void OnCollisionExit(NE::Scripting::Entity other) override {}
    void OnTriggerEnter(NE::Scripting::Entity other) override {}
    void OnTriggerExit(NE::Scripting::Entity other) override {}

    const char* GetTypeName() const override { return "ExampleMaterialScript"; }
};

/**
 * ============================================================================
 * USAGE IN EDITOR
 * ============================================================================
 *
 * 1. Add this script to an entity
 * 2. In the Inspector panel, under Script Fields section, you'll see:
 *    - "primaryMaterial (Material)" - Click the button or drag a .nanomat file from Asset Browser
 *    - "materialList [0]" - Click + to add materials, drag .nanomat files to each element
 *    - "legacyMaterialUUID" - Text field (old way, still works)
 *
 * 3. To assign a material:
 *    - Open Asset Browser panel
 *    - Find a .nanomat file
 *    - Drag it onto the material field button in the Inspector
 *
 * 4. To clear a material:
 *    - Click the X button next to the field
 *
 * ============================================================================
 * BENEFITS OF MaterialRef vs String
 * ============================================================================
 * - Type safety: Can't accidentally assign wrong data
 * - Editor integration: Drag-drop support, shows material names instead of UUIDs
 * - Better validation: Invalid materials show as [Invalid] in editor
 * - Easier to use: No need to manually copy/paste UUIDs
 *
 * ============================================================================
 * HOW TO USE IN YOUR OWN SCRIPTS
 * ============================================================================
 *
 * For SINGLE MaterialRef:
 * -----------------------
 * class MyScript : public NE::Scripting::IScript {
 *     NE::Scripting::MaterialRef myMaterial;
 *
 *     void Initialize(Entity entity) override {
 *         RegisterMaterialRefField("myMaterial", &myMaterial);
 *     }
 * };
 *
 * For VECTOR<MaterialRef>:
 * --------------------------------------
 * class MyScript : public NE::Scripting::IScript {
 *     std::vector<NE::Scripting::MaterialRef> myMaterials;
 *
 *     void Initialize(Entity entity) override {
 *         RegisterMaterialRefVectorField("myMaterials", &myMaterials);
 *     }
 * };
 *
 * 
 *
 * Using MaterialRef in Update():
 * ------------------------------
 * void Update(double deltaTime) override {
 *     // Single material
 *     if (myMaterial.IsValid()) {
 *         NE::Renderer::Command::AssignMaterial(GetEntity(), myMaterial);
 *     }
 *
 *     // Vector material
 *     if (!myMaterials.empty() && myMaterials[0].IsValid()) {
 *         NE::Renderer::Command::AssignMaterial(GetEntity(), myMaterials[0]);
 *     }
 * }
 */
