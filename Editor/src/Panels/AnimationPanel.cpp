#include "AnimationPanel.hpp"

#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <concepts>

#include <Events/EventBus.hpp>
//#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/Animator.hpp>
#include <Math/Vec4.hpp>

#include "../AssetManagement/AssetManager.hpp"
#include "../AssetManagement/Assets/AnimationClipAsset.hpp"
#include "../EditorScene.hpp"
#include "../EditorEvents.hpp"

namespace Editor {
    namespace {
        inline float Clamp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

        inline float ClipLength(const std::shared_ptr<NE::Animation::AnimationClip>& clip) {
            return clip ? clip->GetLengthSeconds() : 0.0f;
        }
        inline bool ClipLooping(const std::shared_ptr<NE::Animation::AnimationClip>& clip) {
            return clip ? clip->IsLooping() : false;
        }

        template <typename T>
        static bool TryAnimValueType(NE::Animation::AnimValueType& out) {
            if constexpr (std::is_same_v<T, bool>) { out = NE::Animation::AnimValueType::Bool; return true; } 
            else if constexpr (std::is_same_v<T, float>) { out = NE::Animation::AnimValueType::Float; return true; }
            else if constexpr (std::is_same_v<T, NE::Math::Vec2>) { out = NE::Animation::AnimValueType::Vec2; return true; } 
            else if constexpr (std::is_same_v<T, NE::Math::Vec3>) { out = NE::Animation::AnimValueType::Vec3; return true; } 
            else if constexpr (std::is_same_v<T, NE::Math::Vec4>) { out = NE::Animation::AnimValueType::Vec4; return true; }
            else return false;
        }

        inline int ChannelCount(NE::Animation::AnimValueType t) {
            switch (t) {
            case NE::Animation::AnimValueType::Bool:  return 1;
            case NE::Animation::AnimValueType::Float: return 1;
            case NE::Animation::AnimValueType::Vec2:  return 2;
            case NE::Animation::AnimValueType::Vec3:  return 3;
            case NE::Animation::AnimValueType::Vec4:  return 4;
            case NE::Animation::AnimValueType::Quat:  return 4;
            default: return 1;
            }
        }

        inline const char* ValueTypeName(NE::Animation::AnimValueType t) {
            switch (t) {
            case NE::Animation::AnimValueType::Bool:  return "Bool";
            case NE::Animation::AnimValueType::Float: return "Float";
            case NE::Animation::AnimValueType::Vec2:  return "Vec2";
            case NE::Animation::AnimValueType::Vec3:  return "Vec3";
            case NE::Animation::AnimValueType::Vec4:  return "Vec4";
            case NE::Animation::AnimValueType::Quat:  return "Quat";
            default: return "Unknown";
            }
        }

        inline const NE::Animation::AnimCurveF* GetCurveByChannel(const NE::Animation::AnimTrack& tr, int ch) {
            switch (ch) {
            case 0: return &tr.x;
            case 1: return &tr.y;
            case 2: return &tr.z;
            case 3: return &tr.w;
            default: return &tr.x;
            }
        }

        inline NE::Animation::AnimCurveF* GetCurveByChannel(NE::Animation::AnimTrack& tr, int ch) {
            switch (ch) {
            case 0: return &tr.x;
            case 1: return &tr.y;
            case 2: return &tr.z;
            case 3: return &tr.w;
            default: return &tr.x;
            }
        }

        inline void CollectUniqueKeyTimes(const NE::Animation::AnimTrack& tr, std::vector<float>& outTimes) {
            outTimes.clear();
            const int channels = ChannelCount(tr.type);

            for (int ch = 0; ch < channels; ++ch) {
                const NE::Animation::AnimCurveF* c = GetCurveByChannel(tr, ch);
                for (const auto& k : c->keys) outTimes.push_back(k.time);
            }

            std::sort(outTimes.begin(), outTimes.end());

            constexpr float eps = 1e-4f;
            outTimes.erase(std::unique(outTimes.begin(), outTimes.end(),
                [&](float a, float b) { return std::fabs(a - b) <= eps; }),
                outTimes.end());
        }

        uint32_t MakeFieldId(const char* componentName, std::string_view fieldName) {
            std::string full;
            full.reserve(std::strlen(componentName) + 1 + fieldName.size());
            full.append(componentName);
            full.push_back('.');
            full.append(fieldName.data(), fieldName.size());
            return FNV1a32(full);
        }

        static void InsertOrUpdateKey(NE::Animation::AnimCurveF& c, float time, float value) {
            constexpr float eps = 1e-4f;

            // overwrite if key exists at time
            for (auto& k : c.keys) {
                if (std::fabs(k.time - time) <= eps) {
                    k.time = time;
                    k.value = value;
                    k.inTan = 0.0f;
                    k.outTan = 0.0f;
                    return;
                }
            }

            c.keys.push_back(NE::Animation::AnimKeyF{ time, value, 0.0f, 0.0f });
            std::sort(c.keys.begin(), c.keys.end(),
                [](const NE::Animation::AnimKeyF& a, const NE::Animation::AnimKeyF& b) { return a.time < b.time; });
        }

        void RecordValueIntoTrack(NE::Animation::AnimTrack& tr, float time, bool v) {
            InsertOrUpdateKey(tr.x, time, v ? 1.0f : 0.0f);
        }

        void RecordValueIntoTrack(NE::Animation::AnimTrack& tr, float time, float v) {
            InsertOrUpdateKey(tr.x, time, v);
        }

        void RecordValueIntoTrack(NE::Animation::AnimTrack& tr, float time, const NE::Math::Vec2& v) {
            InsertOrUpdateKey(tr.x, time, v.x);
            InsertOrUpdateKey(tr.y, time, v.y);
        }

        void RecordValueIntoTrack(NE::Animation::AnimTrack& tr, float time, const NE::Math::Vec3& v) {
            InsertOrUpdateKey(tr.x, time, v.x);
            InsertOrUpdateKey(tr.y, time, v.y);
            InsertOrUpdateKey(tr.z, time, v.z);
        }

        void RecordValueIntoTrack(NE::Animation::AnimTrack& tr, float time, const NE::Math::Vec4& v) {
            InsertOrUpdateKey(tr.x, time, v.x);
            InsertOrUpdateKey(tr.y, time, v.y);
            InsertOrUpdateKey(tr.z, time, v.z);
            InsertOrUpdateKey(tr.w, time, v.w);
        }

        template <typename T>
        struct AnimFieldTrait {
            static constexpr bool supported = false;
        };

        template <> struct AnimFieldTrait<bool> {
            static constexpr bool supported = true;
            static constexpr NE::Animation::AnimValueType type = NE::Animation::AnimValueType::Bool;
        };
        template <> struct AnimFieldTrait<float> {
            static constexpr bool supported = true;
            static constexpr NE::Animation::AnimValueType type = NE::Animation::AnimValueType::Float;
        };
        template <> struct AnimFieldTrait<NE::Math::Vec2> {
            static constexpr bool supported = true;
            static constexpr NE::Animation::AnimValueType type = NE::Animation::AnimValueType::Vec2;
        };
        template <> struct AnimFieldTrait<NE::Math::Vec3> {
            static constexpr bool supported = true;
            static constexpr NE::Animation::AnimValueType type = NE::Animation::AnimValueType::Vec3;
        };
        template <> struct AnimFieldTrait<NE::Math::Vec4> {
            static constexpr bool supported = true;
            static constexpr NE::Animation::AnimValueType type = NE::Animation::AnimValueType::Vec4;
        };

        template <typename T>
        concept AnimatableField = AnimFieldTrait<std::remove_cvref_t<T>>::supported;

        template <AnimatableField T>
        void RecordValueIntoTrack(NE::Animation::AnimTrack& tr, float time, const T& v) {
            if constexpr (std::is_same_v<T, bool>) {
                InsertOrUpdateKey(tr.x, time, v ? 1.0f : 0.0f);
            } else if constexpr (std::is_same_v<T, float>) {
                InsertOrUpdateKey(tr.x, time, v);
            } else if constexpr (std::is_same_v<T, NE::Math::Vec2>) {
                InsertOrUpdateKey(tr.x, time, v.x);
                InsertOrUpdateKey(tr.y, time, v.y);
            } else if constexpr (std::is_same_v<T, NE::Math::Vec3>) {
                InsertOrUpdateKey(tr.x, time, v.x);
                InsertOrUpdateKey(tr.y, time, v.y);
                InsertOrUpdateKey(tr.z, time, v.z);
            } else if constexpr (std::is_same_v<T, NE::Math::Vec4>) {
                InsertOrUpdateKey(tr.x, time, v.x);
                InsertOrUpdateKey(tr.y, time, v.y);
                InsertOrUpdateKey(tr.z, time, v.z);
                InsertOrUpdateKey(tr.w, time, v.w);
            }
        }

        template <typename CompT>
        bool RecordTrackFromComponent(uint32_t entity, NE::Animation::AnimTrack& tr, float time, const char* componentName) {
            CompT& comp = NE::ECS::Command::GetComponent<CompT>(entity);

            bool wrote = false;

            NE::Core::ForEachFieldView(comp, [&](auto&& desc, auto&& fieldValue) {
                if (wrote) return;

                const uint32_t fid = MakeFieldId(componentName, desc.name);
                if (fid != tr.fieldId) return;

                using FieldT = std::remove_cvref_t<decltype(fieldValue)>;

                if constexpr (!AnimFieldTrait<FieldT>::supported) {
                    return;
                } else {
                    if (AnimFieldTrait<FieldT>::type != tr.type) return;

                    RecordValueIntoTrack<FieldT>(tr, time, (const FieldT&)fieldValue);
                    wrote = true;
                }
                });

            return wrote;
        }
    }

    AnimationPanel::AnimationPanel() {
        compEntries = {
            { NE::ECS::Query::GetTransformComponentType(),  "Transform",    &AnimationPanel::Menu_Transform },
            { NE::ECS::Query::GetRendererComponentType(),   "Renderer",     &AnimationPanel::Menu_Renderer  },
            { NE::ECS::Query::GetLightComponentType(),      "Light",        &AnimationPanel::Menu_Light     }
		};

        NANOEngine::Events::EventBus::Get().Subscribe<Events::AutoKeyRecordEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::AutoKeyRecordEvent& e) {
				AutoKeyIfRecording(e.componentTypeId, e.fieldId);
            }
        );
    }

    void AnimationPanel::OnImGuiRender() {
        if (ImGui::Begin("Animation")) {

            if (EditorScene::s_selection.GetLastClicked() != NE::ECS::NO_ENTITY) {
                if (m_selectedEntity != EditorScene::s_selection.GetLastClicked() 
                    && NE::ECS::Query::HasAnimator(EditorScene::s_selection.GetLastClicked())) {
                    m_selectedEntity = EditorScene::s_selection.GetLastClicked();

					auto& animator = NE::ECS::Query::GetEntityAnimator(m_selectedEntity);
                    m_loadedClipPath = Assets::AssetManager::GetInstance().RetrieveFilename(animator.animClipUUID);
                    m_loadedClip = NE::ECS::Command::GetAnimationClip(animator.animClipUUID);
				}
            } else {
                m_selectedEntity = NE::ECS::NO_ENTITY;
                m_loadedClipPath = "";
                m_loadedClip = nullptr;
            }

            const bool hasClip = (m_loadedClip != nullptr);
            if (!hasClip) m_state.playing = false;

            ImGui::BeginChild("AnimLeft", ImVec2(320.0f, 0.0f), true);
            DrawLeftPanel(hasClip);
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("AnimRight", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_NoScrollWithMouse);
            DrawRightDopesheet(hasClip);
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void AnimationPanel::SetTime(float t) {
        if (!m_loadedClip) {
            m_state.time = std::max(0.0f, t);
            return;
        }

        const float L = m_loadedClip->GetLengthSeconds();
        if (L <= 0.0f) { m_state.time = 0.0f; return; }

        if (m_loadedClip->IsLooping()) {
            t = std::fmod(t, L);
            if (t < 0.0f) t += L;
            m_state.time = t;
        } else {
            m_state.time = Clamp(t, 0.0f, L);
        }
    }

    void AnimationPanel::StepToPrevKey() {
        if (!m_loadedClip) return;

        const auto& tracks = m_loadedClip->GetTracks();
        float best = -1.0f;

        std::vector<float> times;
        for (const auto& tr : tracks) {
            CollectUniqueKeyTimes(tr, times);
            for (float kt : times) {
                if (kt < m_state.time && kt > best) best = kt;
            }
        }
        if (best >= 0.0f) SetTime(best);
    }

    void AnimationPanel::StepToNextKey() {
        if (!m_loadedClip) return;

        const auto& tracks = m_loadedClip->GetTracks();
        float best = std::numeric_limits<float>::infinity();

        std::vector<float> times;
        for (const auto& tr : tracks) {
            CollectUniqueKeyTimes(tr, times);
            for (float kt : times) {
                if (kt > m_state.time && kt < best) best = kt;
            }
        }
        if (std::isfinite(best)) SetTime(best);
    }

    bool AnimationPanel::HitDiamond(const ImVec2& p, const ImVec2& c, float r) const {
        float dx = std::fabs(p.x - c.x);
        float dy = std::fabs(p.y - c.y);
        return (dx + dy) <= (r * 1.2f);
    }

    void AnimationPanel::DrawDiamond(ImDrawList* dl, const ImVec2& c, float r, unsigned int col) const {
        ImVec2 a{ c.x,     c.y - r };
        ImVec2 b{ c.x + r, c.y };
        ImVec2 d{ c.x - r, c.y };
        ImVec2 e{ c.x,     c.y + r };
        dl->AddQuadFilled(a, b, e, d, col);
    }

    void AnimationPanel::DrawRulerAndGrid(const ImRect& rect, float pxPerSec, float t0, float t1) {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        float targetPx = 100.0f;
        float step = targetPx / pxPerSec;

        float pow10 = std::pow(10.0f, std::floor(std::log10(std::max(step, 1e-6f))));
        float n = step / pow10;
        float snapped = (n < 2.0f) ? 1.0f : (n < 5.0f ? 2.0f : 5.0f);
        step = snapped * pow10;

        dl->AddRectFilled(rect.Min, rect.Max, IM_COL32(20, 20, 20, 255));
        dl->AddLine(ImVec2(rect.Min.x, rect.Max.y), ImVec2(rect.Max.x, rect.Max.y), IM_COL32(60, 60, 60, 255), 1.0f);

        float start = std::floor(t0 / step) * step;
        for (float t = start; t <= t1 + step; t += step) {
            float x = rect.Min.x + (t - t0) * pxPerSec;
            if (x < rect.Min.x - 1.0f || x > rect.Max.x + 1.0f) continue;

            dl->AddLine(ImVec2(x, rect.Min.y), ImVec2(x, rect.Max.y), IM_COL32(50, 50, 50, 255), 1.0f);

            char buf[32];
            if (step >= 1.0f)       snprintf(buf, sizeof(buf), "%.0fs", t);
            else if (step >= 0.1f)  snprintf(buf, sizeof(buf), "%.1fs", t);
            else                    snprintf(buf, sizeof(buf), "%.2fs", t);

            dl->AddText(ImVec2(x + 3.0f, rect.Min.y + 3.0f), IM_COL32(180, 180, 180, 255), buf);
        }
    }

    void AnimationPanel::DrawPlayhead(const ImRect& rect, float pxPerSec, float t0) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float x = rect.Min.x + (m_state.time - t0) * pxPerSec;
        dl->AddLine(ImVec2(x, rect.Min.y), ImVec2(x, rect.Max.y), IM_COL32(255, 120, 60, 255), 2.0f);
    }

    KeyRef AnimationPanel::PickKeyAt(NE::Animation::AnimTrack& tr, int trackIndex, const ImRect& rowRect, float pxPerSec, float t0, const ImVec2& mouse) {
        const float r = 5.0f;
        const float y = (rowRect.Min.y + rowRect.Max.y) * 0.5f;

        const int channels = ChannelCount(tr.type);
        for (int ch = 0; ch < channels; ++ch) {
            NE::Animation::AnimCurveF* c = GetCurveByChannel(tr, ch);
            for (int i = 0; i < (int)c->keys.size(); ++i) {
                float t = c->keys[i].time;
                float x = rowRect.Min.x + (t - t0) * pxPerSec;
                ImVec2 center{ x, y };
                if (HitDiamond(mouse, center, r)) {
                    return KeyRef{ trackIndex, ch, i };
                }
            }
        }
        return {};
    }

    void AnimationPanel::DrawLeftPanel(bool hasClip) {
        if (!hasClip) {
            ImGui::TextDisabled("No clip loaded.");
            ImGui::Spacing();
        }

        // Left panel should be disabled when no clip
        ImGui::BeginDisabled(!hasClip);

        // Transport row
        ImGui::Checkbox("Preview", &m_state.preview);
        ImGui::SameLine();
        ImGui::Checkbox("Record", &m_state.record);

        ImGui::SameLine();
        ImGui::BeginDisabled(!m_state.record || m_state.selectedTrack < 0);
        if (ImGui::Button("Key")) {
            RecordKeyForTrack(m_state.selectedTrack);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            //auto rec = Assets::AssetManager::GetInstance().GetRecordBySource(m_loadedClipPath);
			//dynamic_cast<Assets::AnimationClipAsset*>(rec->asset.get())->SaveImportSettings(m_loadedClipPath);
        }

        auto smallBtn = [&](const char* label) { return ImGui::Button(label, ImVec2(36, 0)); };

        const float L = hasClip ? m_loadedClip->GetLengthSeconds() : 0.0f;

        if (smallBtn("|<")) SetTime(0.0f);
        ImGui::SameLine();
        if (smallBtn("<K")) StepToPrevKey();
        ImGui::SameLine();
        if (smallBtn(m_state.playing ? "||" : ">")) m_state.playing = !m_state.playing;
        ImGui::SameLine();
        if (smallBtn("K>")) StepToNextKey();
        ImGui::SameLine();
        if (smallBtn(">|")) SetTime(L);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Clip info
        ImGui::TextUnformatted("Clip");
        ImGui::Text("Name: %s", hasClip ? m_loadedClip->GetName().c_str() : "(none)");
        ImGui::Text("Len: %.3fs", hasClip ? m_loadedClip->GetLengthSeconds() : 0.0f);
        ImGui::Text("Time: %.3fs", m_state.time);
        ImGui::SliderFloat("Speed", &m_state.playbackSpeed, 0.1f, 3.0f, "%.2f");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float windowWidth = ImGui::GetWindowSize().x;
        float buttonWidth = ImGui::CalcTextSize("Add Component").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float centeredPosX = (windowWidth - buttonWidth) * 0.5f;
        ImGui::SetCursorPosX(centeredPosX);

        if (ImGui::Button("Add Property")) {
            ImGui::OpenPopup("ComponentList");
        }

        if (hasClip) {
            NE::ECS::Signature sig(NE::ECS::Query::GetEntitySignature(m_selectedEntity));
            if (ImGui::BeginPopup("ComponentList")) {
                for (const auto& entry : compEntries) {
                    if (!sig.test(entry.componentTypeId)) continue;

                    if (ImGui::BeginMenu(entry.label)) {
                        (this->*entry.drawFieldMenu)(m_selectedEntity);
                        ImGui::EndMenu();
                    }
                }
                ImGui::EndPopup();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Selected key info (can keep even if clip exists)
        ImGui::TextUnformatted("Selected Key");
        if (m_state.selectedKey.trackIndex >= 0) {
            ImGui::Text("Track: %d  Ch: %d  Key: %d",
                m_state.selectedKey.trackIndex,
                m_state.selectedKey.channel,
                m_state.selectedKey.keyIndex);
        } else {
            ImGui::TextUnformatted("(none)");
        }

        ImGui::EndDisabled();
    }

    void AnimationPanel::DrawRightDopesheet(bool hasClip) {
        ImGuiIO& io = ImGui::GetIO();

        // Playback only if clip exists
        if (hasClip && m_state.playing) {
            SetTime(m_state.time + io.DeltaTime * m_state.playbackSpeed);
            if (!m_loadedClip->IsLooping() && m_state.time >= m_loadedClip->GetLengthSeconds())
                m_state.playing = false;
        } else if (!hasClip) {
            m_state.playing = false;
        }

        if (hasClip && m_state.record && m_state.selectedTrack >= 0) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                ImGui::IsKeyPressed(ImGuiKey_K)) {
                RecordKeyForTrack(m_state.selectedTrack);
            }
        }

        // Zoom (UI-only; allowed always)
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
            if (io.KeyCtrl && io.MouseWheel != 0.0f) {
                float oldPx = m_state.zoom;
                float newPx = Clamp(oldPx * (1.0f + io.MouseWheel * 0.1f), 20.0f, 800.0f);
                m_state.zoom = newPx;
            }
        }

        const float pxPerSec = m_state.zoom;

        ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("DopesheetTable", 2, flags)) {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 260.0f);
            ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            // Ruler row
            ImGui::TableNextRow(ImGuiTableRowFlags_None, 28.0f);
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Timeline");

            ImGui::TableSetColumnIndex(1);
            ImVec2 rulerSize = ImGui::GetContentRegionAvail();
            rulerSize.y = 26.0f;

            ImVec2 rulerPos = ImGui::GetCursorScreenPos();
            ImRect rulerRect(rulerPos, ImVec2(rulerPos.x + rulerSize.x, rulerPos.y + rulerSize.y));

            float t0 = m_state.viewStart;
            float t1 = t0 + (rulerSize.x / pxPerSec);

            DrawRulerAndGrid(rulerRect, pxPerSec, t0, t1);
            DrawPlayhead(rulerRect, pxPerSec, t0);

            ImGui::InvisibleButton("RulerScrub", rulerSize);

            // Scrub: if clip exists, clamp/wrap; else just move playhead visually
            if (ImGui::IsItemActive() || (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
                float mx = io.MousePos.x;
                float t = t0 + (mx - rulerRect.Min.x) / pxPerSec;
                SetTime(t);
            }

            // Pan (always)
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
                    m_state.viewStart -= io.MouseDelta.x / pxPerSec;
                    m_state.viewStart = std::max(0.0f, m_state.viewStart);
                }
                if (io.KeyShift && io.MouseWheel != 0.0f) {
                    m_state.viewStart -= io.MouseWheel * (120.0f / pxPerSec);
                    m_state.viewStart = std::max(0.0f, m_state.viewStart);
                }
            }

            // If no clip, show empty state and exit after ruler
            if (!hasClip) {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, 26.0f);
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("-");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("No clip loaded.");
                ImGui::EndTable();
                return;
            }

            // Track rows
            auto& tracks = m_loadedClip->GetTracksMutable();
            const float rowH = m_state.rowHeight;

            for (int ti = 0; ti < (int)tracks.size(); ++ti) {
                NE::Animation::AnimTrack& tr = tracks[ti];

                ImGui::TableNextRow(ImGuiTableRowFlags_None, rowH);

                ImGui::TableSetColumnIndex(0);
                std::string label = tr.relativePath + "##track_" + std::to_string(ti);
                if (ImGui::Selectable(label.c_str(), m_state.selectedTrack == ti, ImGuiSelectableFlags_SpanAllColumns)) {
                    m_state.selectedTrack = ti;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", ValueTypeName(tr.type));

                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("...row drawing...");
            }

            ImGui::EndTable();
        }
    }

    bool AnimationPanel::RecordKeyForTrack(int trackIndex) {
        if (!m_loadedClip) return false;
        if (m_selectedEntity == NE::ECS::NO_ENTITY) return false;

        auto& tracks = m_loadedClip->GetTracksMutable();
        if (trackIndex < 0 || trackIndex >= (int)tracks.size()) return false;

        auto& tr = tracks[trackIndex];
        const float time = m_state.time;
        
        if (tr.componentTypeId == NE::ECS::Query::GetTransformComponentType()) {
            return RecordTrackFromComponent<NE::ECS::Component::Transform>(m_selectedEntity, tr, time, "Transform");
        }
        if (tr.componentTypeId == NE::ECS::Query::GetRendererComponentType()) {
            return RecordTrackFromComponent<NE::ECS::Component::Renderer>(m_selectedEntity, tr, time, "Renderer");
        }
        if (tr.componentTypeId == NE::ECS::Query::GetLightComponentType()) {
            return RecordTrackFromComponent<NE::ECS::Component::Light>(m_selectedEntity, tr, time, "Light");
        }

        return false;
    }

    bool AnimationPanel::AutoKeyIfRecording(uint32_t componentTypeId, uint32_t fieldId) {
        if (!m_loadedClip || !m_state.record) return false;
        if (m_selectedEntity == NE::ECS::NO_ENTITY) return false;

        auto& tracks = m_loadedClip->GetTracksMutable();

        bool keyedAny = false;
        for (int i = 0; i < (int)tracks.size(); ++i) {
            auto& tr = tracks[i];
            if (tr.componentTypeId != componentTypeId) continue;
            if (tr.fieldId != fieldId) continue;

            keyedAny |= RecordKeyForTrack(i);
        }
        return keyedAny;
    }

    void AnimationPanel::Menu_Transform(uint32_t e) {
        DrawAnimFieldMenuForComponent<NE::ECS::Component::Transform>(
            e, NE::ECS::Query::GetTransformComponentType(), "Transform");
    }

    void AnimationPanel::Menu_Renderer(uint32_t e) {
        DrawAnimFieldMenuForComponent<NE::ECS::Component::Renderer>(
            e, NE::ECS::Query::GetRendererComponentType(), "Renderer");
    }

    void AnimationPanel::Menu_Light(uint32_t e) {
        DrawAnimFieldMenuForComponent<NE::ECS::Component::Light>(
            e, NE::ECS::Query::GetLightComponentType(), "Light");
    }

}
