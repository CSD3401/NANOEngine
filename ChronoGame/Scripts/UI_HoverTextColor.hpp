#pragma once
#include "EngineAPI.hpp"
#include <ScriptSDK/UI.h>

/**
 * UI_HoverTextColor
 * -----------------
 * Attach this script to an entity with UIButton.
 * Assign `targetText` to the UIText entity to recolor.
 *
 * Behavior:
 * - hovered   -> hover color
 * - not hover -> normal color
 */
class UI_HoverTextColor : public IScript {
public:
    UI_HoverTextColor() {
        SCRIPT_GAMEOBJECT_REF(targetText);
        SCRIPT_FIELD(normalHex, String);
        SCRIPT_FIELD(hoverHex, String);
    }

    ~UI_HoverTextColor() override = default;

    void Awake() override {}
    void Initialize(Entity entity) override { m_buttonEntity = entity; }

    void Start() override {
        if (!NE::ECS::Query::HasUIButton(m_buttonEntity)) {
            LOG_WARNING("UI_HoverTextColor: entity is missing UIButton");
            return;
        }
        if (!targetText.IsValid() || !NE::ECS::Query::HasUIText(targetText.GetEntity())) {
            LOG_WARNING("UI_HoverTextColor: targetText is invalid or missing UIText");
            return;
        }

        RefreshParsedColors();
        ApplyNormal();
    }

    void Update(double /*dt*/) override {
        if (!NE::ECS::Query::HasUIButton(m_buttonEntity)) return;
        if (!targetText.IsValid() || !NE::ECS::Query::HasUIText(targetText.GetEntity())) return;

        RefreshParsedColors();

        const bool hovered = UI::IsButtonHovered(m_buttonEntity);
        if (hovered == m_wasHoveredLastFrame) return;

        m_wasHoveredLastFrame = hovered;
        if (hovered) {
            ApplyHover();
            PlayAudio("Event:/MAIN_MENU/BUTTON_HOVER");
        }
        else ApplyNormal();
    }

    void OnDestroy() override {}
    void OnEnable() override {}
    void OnDisable() override {}
    void OnValidate() override {}

    const char* GetTypeName() const override { return "UI_HoverTextColor"; }

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

private:
    Entity m_buttonEntity = 0;
    GameObjectRef targetText;
    bool m_wasHoveredLastFrame = false;

    // Accepts #RRGGBB or #RRGGBBAA (also works without '#').
    std::string normalHex = "#FFFFFFFF";
    std::string hoverHex = "#FFE633FF";

    float m_normalR = 1.0f;
    float m_normalG = 1.0f;
    float m_normalB = 1.0f;
    float m_normalA = 1.0f;

    float m_hoverR = 1.0f;
    float m_hoverG = 0.9f;
    float m_hoverB = 0.2f;
    float m_hoverA = 1.0f;

    static int HexNibble(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    }

    static int ParseHexByte(char hi, char lo) {
        const int h = HexNibble(hi);
        const int l = HexNibble(lo);
        if (h < 0 || l < 0) return -1;
        return (h << 4) | l;
    }

    bool TryParseHexColor(const std::string& hex, float& r, float& g, float& b, float& a) const {
        std::string s = hex;
        if (!s.empty() && s[0] == '#') s.erase(0, 1);
        if (s.size() != 6 && s.size() != 8) return false;

        const int rr = ParseHexByte(s[0], s[1]);
        const int gg = ParseHexByte(s[2], s[3]);
        const int bb = ParseHexByte(s[4], s[5]);
        const int aa = (s.size() == 8) ? ParseHexByte(s[6], s[7]) : 255;
        if (rr < 0 || gg < 0 || bb < 0 || aa < 0) return false;

        r = static_cast<float>(rr) / 255.0f;
        g = static_cast<float>(gg) / 255.0f;
        b = static_cast<float>(bb) / 255.0f;
        a = static_cast<float>(aa) / 255.0f;
        return true;
    }

    void RefreshParsedColors() {
        float r, g, b, a;
        if (TryParseHexColor(normalHex, r, g, b, a)) {
            m_normalR = r; m_normalG = g; m_normalB = b; m_normalA = a;
        }
        else {
            LOG_WARNING("UI_HoverTextColor: invalid normalHex '" << normalHex
                << "'. Use #RRGGBB or #RRGGBBAA");
        }

        if (TryParseHexColor(hoverHex, r, g, b, a)) {
            m_hoverR = r; m_hoverG = g; m_hoverB = b; m_hoverA = a;
        }
        else {
            LOG_WARNING("UI_HoverTextColor: invalid hoverHex '" << hoverHex
                << "'. Use #RRGGBB or #RRGGBBAA");
        }
    }

    void ApplyNormal() {
        NE::ECS::Command::SetUITextColor(targetText.GetEntity(), m_normalR, m_normalG, m_normalB, m_normalA);
    }

    void ApplyHover() {
        NE::ECS::Command::SetUITextColor(targetText.GetEntity(), m_hoverR, m_hoverG, m_hoverB, m_hoverA);
    }
};