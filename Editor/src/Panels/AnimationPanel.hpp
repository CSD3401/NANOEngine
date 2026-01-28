#pragma once
#include "IPanel.hpp"

#include <memory>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <Animation/AnimationClip.hpp>
#include <EditorInterface/ECSExports.hpp>
#include <Core/Reflection.hpp>
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Light.hpp>

namespace Editor {
    namespace {
        inline uint32_t FNV1a32(std::string_view s) {
            uint32_t h = 2166136261u;
            for (unsigned char c : s) { h ^= c; h *= 16777619u; }
            return h;
        }
    }

    struct KeyRef {
        int trackIndex = -1;
        int channel = -1;
        int keyIndex = -1;
    };

    struct DopesheetState {
        // transport
        bool preview = false;
        bool record = false;
        bool playing = false;
        float playbackSpeed = 1.0f;

        // time
        float time = 0.0f;

        // view
        float viewStart = 0.0f;     // seconds
        float viewSeconds = 5.0f;   // visible range (seconds)
        float zoom = 120.0f;        // pixels per second (derived-ish)
        float rowHeight = 22.0f;

        // selection
        KeyRef selectedKey{};
        float selectedKeyOriginalTime = 0.0f;
        bool draggingKey = false;

        // add track UI (minimal)
        char addRelativePath[256] = "Root";
        uint32_t addComponentTypeId = 0;
        uint32_t addFieldId = 0;
        int addValueType = (int)NE::Animation::AnimValueType::Vec3;

        int selectedTrack = -1;
    };

    class AnimationPanel final : public IPanel {
    public:
		AnimationPanel();
        void OnImGuiRender() override;

        std::string GetTrackDisplayName(const NE::Animation::AnimTrack& tr) const;

        DopesheetState& State() { return m_state; }
    private:
        struct AnimCompEntry {
            uint32_t componentTypeId;
            const char* label;
            void (AnimationPanel::* drawFieldMenu)(uint32_t e);
        };
        std::vector<AnimCompEntry> compEntries;

        uint32_t m_selectedEntity = UINT32_MAX;
		std::string m_loadedClipPath;
        std::shared_ptr<NE::Animation::AnimationClip> m_loadedClip;

        DopesheetState m_state{};

        void DrawLeftPanel(bool hasClip);
        void DrawRightDopesheet(bool hasClip);

        bool RecordKeyForTrack(int trackIndex);

        bool AutoKeyIfRecording(uint32_t componentTypeId, uint32_t fieldId);

        void DrawTrackKeys(ImDrawList* dl, NE::Animation::AnimTrack& tr, int ti, const ImRect& rowRect, float pxPerSec, float t0);

        void SetTime(float t);
        void StepToPrevKey();
        void StepToNextKey();

        void DrawRulerAndGrid(const ImRect& rect, float pxPerSec, float t0, float t1);
        void DrawPlayhead(const ImRect& rect, float pxPerSec, float t0);

        bool HitDiamond(const ImVec2& p, const ImVec2& center, float r) const;
        void DrawDiamond(ImDrawList* dl, const ImVec2& c, float r, unsigned int col) const;

        KeyRef PickKeyAt(NE::Animation::AnimTrack& tr, int trackIndex, const ImRect& rowRect, float pxPerSec, float t0, const ImVec2& mouse);

        void MenuEntityMeta(uint32_t e);
        void MenuTransform(uint32_t e);
        void MenuRenderer(uint32_t e);
        void MenuLight(uint32_t e);

        template <typename C>
        void DrawAnimFieldMenuForComponent(uint32_t e, uint32_t componentTypeId, const char* componentName) {
            // get component reference (you must adapt this line to your ECS API)
            const C& comp = NE::ECS::Query::GetComponent<C>(e);

            NE::Core::ForEachFieldView(comp, [&](auto&& desc, auto&& fieldValue) {
                using FieldT = std::remove_cvref_t<decltype(fieldValue)>;

                NE::Animation::AnimValueType animType;
                if (!TryAnimValueType<FieldT>(animType))
                    return; // skip non animatable types

                // stable field id (hash of Component.Field)
                std::string full;
                full.reserve(std::strlen(componentName) + 1 + desc.name.size());
                full.append(componentName);
                full.push_back('.');
                full.append(desc.name.data(), desc.name.size());
                const uint32_t fieldId = FNV1a32(full);

                // show item
                if (ImGui::MenuItem(full.c_str())) {
                    // fill your add-track state
                    m_state.addComponentTypeId = componentTypeId;
                    m_state.addFieldId = fieldId;
                    m_state.addValueType = (int)animType;

                    // optional: auto-fill relative path from selection
                    // snprintf(m_state.addRelativePath, sizeof(m_state.addRelativePath), "%s", ComputeRelativePath(...).c_str());

                    // immediately add track (or keep selection and let user press Add Track)
                    if (m_loadedClip) {
                        auto& tracks = m_loadedClip->GetTracksMutable();
                        NE::Animation::AnimTrack tr{};
                        tr.relativePath = m_state.addRelativePath;
                        tr.componentTypeId = componentTypeId;
                        tr.fieldId = fieldId;
                        tr.type = animType;
                        tracks.push_back(std::move(tr));
                    }

                    ImGui::CloseCurrentPopup();
                }
                });
        }
    };

}
