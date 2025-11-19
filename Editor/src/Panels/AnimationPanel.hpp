#pragma once
#include "IPanel.hpp"
#include <imgui/imgui.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>

// Make NE::Animation visible here

namespace NE::Animation {
    struct TransformClip;
}

namespace NE { namespace ECS {
    namespace Component { struct Transform; struct Animator; }
}}

namespace Editor {

    // Forward-declared minimal TransformClip (editor side uses same struct by include path)
    namespace NEAnim = NE::Animation;

    class AnimationPanel : public IPanel {
    public:
        AnimationPanel() = default;
        virtual void OnImGuiRender() override;

    private:
        // Editor state
        float m_currentTime = 0.0f;
        float m_zoom        = 1.0f;   // seconds per 100px
        bool  m_playing     = false;
        bool  m_loop        = true;
        float m_length      = 2.0f;

        // per-entity simple clip store (in-memory)
        std::unordered_map<uint32_t, std::shared_ptr<NEAnim::TransformClip>> m_entityClips;

        // Helpers
        void DrawHeader(NE::ECS::Component::Animator* animator,NEAnim::TransformClip& clip);
        void DrawDopesheet(uint32_t entityId, NE::ECS::Component::Transform& tr, NEAnim::TransformClip& clip);
        void AddKey(NEAnim::TransformClip& clip, float t, const NE::ECS::Component::Transform& tr, bool pos, bool rot, bool scl);
    };

} // namespace Editor
