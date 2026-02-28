#pragma once
#include "EngineAPI.hpp"

/**
 * UI_ButtonHoverSwap
 * ------------------
 * Attach this script to an entity that has a UIButton component.
 *
 * When the cursor hovers the button, it swaps a UIImage's texture to "hoverTextureUUID".
 * When the cursor leaves, it swaps back to "normalTextureUUID".
 *
 * Setup in editor:
 *  - Add UIButton + UIImage on the same entity (common), OR
 *  - Add UIButton on this entity, and drag a separate UIImage entity into "targetImage".
 *  - Fill in hoverTextureUUID, and (optionally) normalTextureUUID.
 *      * If normalTextureUUID is left empty, the script will auto-capture the current
 *        UIImage texture on Start() and use that as the "normal" texture.
 */
class UI_ButtonHoverSwap : public IScript {
public:
    UI_ButtonHoverSwap() {
        SCRIPT_FIELD(targetImage, GameObjectRef);
        SCRIPT_FIELD(normalTextureUUID, String);
        SCRIPT_FIELD(hoverTextureUUID, String);
    }

    ~UI_ButtonHoverSwap() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override {}
    void Start() override {
        m_buttonEntity = GetEntity();

        // Decide which UIImage to drive:
        // - If targetImage is set, use it.
        // - Otherwise, assume the same entity has UIImage.
        m_imageEntity = targetImage.IsValid() ? targetImage.GetEntity() : m_buttonEntity;

        // Auto-capture normal texture if not specified.
        if (normalTextureUUID.empty()) {
            if (NE::ECS::Query::HasUIImage(m_imageEntity)) {
                normalTextureUUID = NE::ECS::Query::GetUIImage(m_imageEntity).textureUUID;
            }
        }

        // Force initial state to "not hovered"
        m_lastHovered = false;
        if (!normalTextureUUID.empty()) {
            Command::SetUIImageTexture(m_imageEntity, normalTextureUUID);
        }
    }

    void Update(double /*deltaTime*/) override {
        if (m_buttonEntity == 0 || m_imageEntity == 0) return;
        if (hoverTextureUUID.empty()) return;

        const bool hovered = UI::IsButtonHovered(m_buttonEntity);

        // Only swap when state changes (avoids re-loading every frame).
        if (hovered != m_lastHovered) {
            if (hovered) {
                Command::SetUIImageTexture(m_imageEntity, hoverTextureUUID);
            }
            else {
                if (!normalTextureUUID.empty()) {
                    Command::SetUIImageTexture(m_imageEntity, normalTextureUUID);
                }
            }
            m_lastHovered = hovered;
        }
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}
    const char* GetTypeName() const override { return "UI_ButtonHoverSwap"; }

    void OnCollisionEnter(Entity) override {}
    void OnCollisionExit(Entity) override {}
    void OnCollisionStay(Entity) override {}
    void OnTriggerEnter(Entity) override {}
    void OnTriggerExit(Entity) override {}
    void OnTriggerStay(Entity) override {}

private:
    // Exposed fields
    GameObjectRef targetImage;          // Optional: UIImage entity to swap
    std::string normalTextureUUID = ""; // Optional: if empty, auto-captured on Start()
    std::string hoverTextureUUID = "";  // Required

    // Runtime cache
    uint32_t m_buttonEntity = 0;
    uint32_t m_imageEntity = 0;
    bool m_lastHovered = false;
};
