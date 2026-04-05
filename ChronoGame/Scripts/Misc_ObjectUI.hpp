#pragma once
#include "EngineAPI.hpp"

// This script is for when the player raycast hits something that they can interact with
// it will show on screen as text, what the object is and how to interact with them
// eg:
// Gate
// Interact [E]
class Misc_ObjectUI : public IScript {
public:
    Misc_ObjectUI()
    {
        SCRIPT_GAMEOBJECT_REF(objectNameTextUI);
        SCRIPT_GAMEOBJECT_REF(objectDescTextUI);
        SCRIPT_FIELD(objectName, String);
        SCRIPT_FIELD(objectDescription, String);
    }
    ~Misc_ObjectUI() override = default;

    // == Custom Methods ==


    // === Lifecycle Methods ===
    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override {}
    void Update(double deltaTime) override
    {


    }
    void OnDestroy() override {}

    // === Optional Callbacks ===
    void OnEnable() override {


    }
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "Misc_ObjectUI"; }

    // === Collision Callbacks ===
    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    void SetUIText()
    {
        LOG_DEBUG("CALLING SET UI TEXT");
        SetActive(true, objectNameTextUI.GetEntity());
        SetActive(true, objectDescTextUI.GetEntity());

        NE::Scripting::SetUIText(objectNameTextUI.GetEntity(), objectName.c_str());
        NE::Scripting::SetUIText(objectDescTextUI.GetEntity(), objectDescription.c_str());
    }

    void ClearText()
    {
        LOG_DEBUG("CALLING CLEAR TEXT UI");
        NE::Scripting::SetUIText(objectNameTextUI.GetEntity(), "");
        NE::Scripting::SetUIText(objectDescTextUI.GetEntity(), "");

        SetActive(false, objectNameTextUI.GetEntity());
        SetActive(false, objectDescTextUI.GetEntity());
    }

private:

    GameObjectRef objectNameTextUI;
    GameObjectRef objectDescTextUI;

    std::string objectName;
    std::string objectDescription;
};