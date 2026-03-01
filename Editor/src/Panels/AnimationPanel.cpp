#include "pch.h"
#include "AnimationPanel.hpp"

#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <concepts>

#include <Events/EventBus.hpp>
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
        bool TryAnimValueType(NE::Animation::AnimValueType& out) {
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

        void InsertOrUpdateKey(NE::Animation::AnimCurveF& c, float time, float value) {
            constexpr float eps = 1e-4f;

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

        void DrawTrackRowBackground(ImDrawList* dl, const ImRect& r, int index) {
            ImU32 bg = (index % 2 == 0) ? IM_COL32(25, 25, 25, 255) : IM_COL32(30, 30, 30, 255);
            dl->AddRectFilled(r.Min, r.Max, bg);
        }

        float ComputeMaxKeyTime(const NE::Animation::AnimationClip& clip) {
            float mx = 0.0f;
            for (auto const& tr : clip.GetTracks()) {
                const int chN = ChannelCount(tr.type);
                for (int ch = 0; ch < chN; ++ch) {
                    auto const* c = GetCurveByChannel(tr, ch);
                    for (auto const& k : c->keys)
                        mx = std::max(mx, k.time);
                }
            }
            return mx;
        }

        struct FieldDisplay {
            std::string component;
            std::string field;
        };

        std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::string>> m_fieldNameByCompAndId;

        template <typename CompT>
        void CacheFieldsForComponent(uint32_t compTypeId, const char* compName) {
            auto& map = m_fieldNameByCompAndId[compTypeId];
            if (!map.empty()) return;

            CompT dummy{};
            NE::Core::ForEachFieldView(dummy, [&](auto&& desc, auto&&) {
                const uint32_t fid = MakeFieldId(compName, desc.name);
                map[fid] = std::string(desc.name);
                });
        }

        float ComputeMinKeyTime(const NE::Animation::AnimationClip& clip) {
            float best = std::numeric_limits<float>::infinity();
            for (const auto& tr : clip.GetTracks()) {
                const int chN = ChannelCount(tr.type);
                for (int ch = 0; ch < chN; ++ch) {
                    const auto* c = GetCurveByChannel(tr, ch);
                    for (const auto& k : c->keys) best = std::min(best, k.time);
                }
            }
            return std::isfinite(best) ? best : 0.0f;
        }

        template <class T>
        void SetBaselineVariant(BaselineEntry& e, const T& v) {
            using U = std::remove_cvref_t<T>;

            if constexpr (std::is_same_v<U, bool>) {
                e.value = v;
            } else if constexpr (std::is_same_v<U, float>) {
                e.value = v;
            } else if constexpr (std::is_same_v<U, NE::Math::Vec2>) {
                e.value = v;
            } else if constexpr (std::is_same_v<U, NE::Math::Vec3>) {
                e.value = v;
            } else if constexpr (std::is_same_v<U, NE::Math::Vec4>) {
                e.value = v;
            }
            else {
                // unsupported type
            }
        }

        template<class T>
        using DecayT = std::remove_cvref_t<T>;

        template<class T>
        concept BaselineSupported =
            std::same_as<DecayT<T>, bool> ||
            std::same_as<DecayT<T>, float> ||
            std::same_as<DecayT<T>, NE::Math::Vec2> ||
            std::same_as<DecayT<T>, NE::Math::Vec3> ||
            std::same_as<DecayT<T>, NE::Math::Vec4>;

        template<class FieldT>
        DecayT<FieldT>* GetBaselinePtr(BaselineEntry& be) {
            if constexpr (BaselineSupported<FieldT>) {
                return std::get_if<DecayT<FieldT>>(&be.value);
            } else {
                return nullptr;
            }
        }

        template<class FieldT>
        const DecayT<FieldT>* GetBaselinePtr(const BaselineEntry& be) {
            if constexpr (BaselineSupported<FieldT>) {
                return std::get_if<DecayT<FieldT>>(&be.value);
            } else {
                return nullptr;
            }
        }


        template <typename CompT>
        static bool CaptureBaselineFromComponent(
            uint32_t entity,
            const NE::Animation::AnimTrack& tr,
            const char* componentName,
            std::vector<BaselineEntry>& out,
            std::unordered_set<BaselineKey, BaselineKeyHash>& dedup
        ) {
            CompT& comp = NE::ECS::Command::GetComponent<CompT>(entity);

            bool wrote = false;
            NE::Core::ForEachFieldView(comp, [&](auto&& desc, auto&& currentValue) {
                if (wrote) return;

                const uint32_t fid = MakeFieldId(componentName, desc.name);
                if (fid != tr.fieldId) return;

                using FieldT = std::remove_cvref_t<decltype(currentValue)>;

                NE::Animation::AnimValueType inferred;
                if (!TryAnimValueType<FieldT>(inferred)) return;
                if (inferred != tr.type) return;

                BaselineKey k{ tr.componentTypeId, tr.fieldId };
                if (dedup.find(k) != dedup.end()) { wrote = true; return; }

                BaselineEntry e{};
                e.componentTypeId = tr.componentTypeId;
                e.fieldId = tr.fieldId;
                e.type = tr.type;
                SetBaselineVariant(e, currentValue);

                out.push_back(std::move(e));
                dedup.insert(k);
                wrote = true;
                });

            return wrote;
        }

        template <typename CompT>
        static bool RestoreBaselineToComponent(
            uint32_t entity,
            const BaselineEntry& be,
            const char* componentName
        ) {
            CompT& comp = NE::ECS::Command::GetComponent<CompT>(entity);

            bool wrote = false;
            NE::Core::ForEachField(comp, [&](auto const& desc, auto&& fieldValue) {
                if (wrote) return;

                const uint32_t fid = MakeFieldId(componentName, desc.name);
                if (fid != be.fieldId) return;

                NE::Animation::AnimValueType inferred;
                using FieldT = std::remove_cvref_t<decltype(fieldValue)>;
                if (!TryAnimValueType<FieldT>(inferred)) return;
                if (inferred != be.type) return;

                if (auto* pv = GetBaselinePtr<FieldT>(be)) {
                    fieldValue = *pv;
                    if constexpr (requires { comp.isDirty; }) comp.isDirty = true;
                    wrote = true;
                }
            });

            return wrote;
        }

    }

    AnimationPanel::AnimationPanel() {
        compEntries = {
            { NE::ECS::Query::GetEntityMetaComponentType(), "EntityMeta",   &AnimationPanel::MenuEntityMeta },
            { NE::ECS::Query::GetTransformComponentType(),  "Transform",    &AnimationPanel::MenuTransform  },
            { NE::ECS::Query::GetRendererComponentType(),   "Renderer",     &AnimationPanel::MenuRenderer   },
            { NE::ECS::Query::GetLightComponentType(),      "Light",        &AnimationPanel::MenuLight      }
		};

        CacheFieldsForComponent<NE::ECS::Component::EntityMeta>(NE::ECS::Query::GetEntityMetaComponentType(), "EntityMeta");
        CacheFieldsForComponent<NE::ECS::Component::Transform>(NE::ECS::Query::GetTransformComponentType(), "Transform");
        CacheFieldsForComponent<NE::ECS::Component::Renderer>(NE::ECS::Query::GetRendererComponentType(), "Renderer");
        CacheFieldsForComponent<NE::ECS::Component::Light>(NE::ECS::Query::GetLightComponentType(), "Light");

        NANOEngine::Events::EventBus::Get().Subscribe<Events::AutoKeyRecordEvent>(
            NANOEngine::Events::EventDomain::Editor,
            [&](const Events::AutoKeyRecordEvent& e) {
				AutoKeyIfRecording(e.componentTypeId, e.fieldId);
            }
        );
    }

    void AnimationPanel::OnImGuiRender() {
        if (ImGui::Begin("Animation")) {
            uint32_t nextEntity = NE::ECS::NO_ENTITY;
            std::string nextClipPath;
            std::shared_ptr<NE::Animation::AnimationClip> nextClip = nullptr;

            const uint32_t clickedEntity = EditorScene::s_selection.GetLastClicked();
            if (clickedEntity != NE::ECS::NO_ENTITY && NE::ECS::Query::HasAnimator(clickedEntity)) {
                nextEntity = clickedEntity;
                auto& animator = NE::ECS::Query::GetEntityAnimator(nextEntity);
                nextClipPath = Assets::AssetManager::GetInstance().RetrieveFilename(animator.animClipUUID);
                nextClip = NE::ECS::Command::GetAnimationClip(animator.animClipUUID);
            }

            const bool contextChanged = (nextEntity != m_selectedEntity) || (nextClip.get() != m_loadedClip.get());
            if (contextChanged && m_previewActive) {
                EndPreview();
            }

            m_selectedEntity = nextEntity;
            m_loadedClipPath = std::move(nextClipPath);
            m_loadedClip = std::move(nextClip);

            if (contextChanged) {
                m_state.selectedTrack = -1;
                m_state.selectedKey = {};
                m_state.draggingKey = false;
            }

            const bool hasClip = (m_loadedClip != nullptr);
            if (!hasClip) {
                m_state.playing = false;
                m_state.preview = false;
            }

            EndPreviewIfContextInvalid();

            ImGui::BeginChild("AnimLeft", ImVec2(300.0f, 0.0f), true);
            DrawLeftPanel(hasClip);
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("AnimRight", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_NoScrollWithMouse);
            DrawRightDopesheet(hasClip);
            ImGui::EndChild();
        }
        ImGui::End();
    }

    std::string AnimationPanel::GetTrackDisplayName(const NE::Animation::AnimTrack& tr) const {
        const char* compName = "?";
        if (tr.componentTypeId == NE::ECS::Query::GetEntityMetaComponentType()) {
            compName = "EntityMeta";
        } else if (tr.componentTypeId == NE::ECS::Query::GetTransformComponentType()) {
            compName = "Transform";
        } else if (tr.componentTypeId == NE::ECS::Query::GetRendererComponentType()) {
            compName = "Renderer";
        } else if (tr.componentTypeId == NE::ECS::Query::GetLightComponentType()) {
            compName = "Light";
        }

        auto itComp = m_fieldNameByCompAndId.find(tr.componentTypeId);
        if (itComp != m_fieldNameByCompAndId.end()) {
            auto itField = itComp->second.find(tr.fieldId);
            if (itField != itComp->second.end()) {
                return std::string(compName) + "." + itField->second;
            }
        }

        return std::string(compName) + ".(unknown)";
    }


    void AnimationPanel::SetTime(float t) {
        if (!m_loadedClip) {
            m_state.time = std::max(0.0f, t);
            return;
        }

        float L = m_loadedClip->GetLengthSeconds();
        if (L <= 0.0f) L = ComputeMaxKeyTime(*m_loadedClip);

        if (m_state.record) {
            if (t < 0.0f) t = 0.0f;

            if (t > L) {
                L = t;
                m_loadedClip->SetLengthSeconds(L);
            }

            m_state.time = t;
            return;
        }

        if (L <= 0.0f) {
            m_state.time = std::max(0.0f, t);
            return;
        }

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

        for (const auto& tr : tracks) {
            std::vector<float> times;
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

        for (const auto& tr : tracks) {
            std::vector<float> times;
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

    bool AnimationPanel::IsSelectedKeyValid() const {
        if (!m_loadedClip) return false;

        const auto& tracks = m_loadedClip->GetTracks();
        if (m_state.selectedKey.trackIndex < 0 || m_state.selectedKey.trackIndex >= (int)tracks.size())
            return false;

        const auto& tr = tracks[m_state.selectedKey.trackIndex];
        const int channels = ChannelCount(tr.type);
        if (m_state.selectedKey.channel < 0 || m_state.selectedKey.channel >= channels)
            return false;

        const auto* c = GetCurveByChannel(tr, m_state.selectedKey.channel);
        if (m_state.selectedKey.keyIndex < 0 || m_state.selectedKey.keyIndex >= (int)c->keys.size())
            return false;

        return true;
    }

    int AnimationPanel::DeleteKeysAtTime(NE::Animation::AnimTrack& tr, float time, bool allChannels) {
        constexpr float eps = 1e-4f;
        int removed = 0;
        const int channels = ChannelCount(tr.type);

        auto eraseAtTime = [&](NE::Animation::AnimCurveF& c) {
            const int before = (int)c.keys.size();
            c.keys.erase(std::remove_if(c.keys.begin(), c.keys.end(),
                [&](const NE::Animation::AnimKeyF& k) { return std::fabs(k.time - time) <= eps; }),
                c.keys.end());
            removed += before - (int)c.keys.size();
        };

        if (!allChannels) {
            if (m_state.selectedKey.channel >= 0 && m_state.selectedKey.channel < channels) {
                eraseAtTime(*GetCurveByChannel(tr, m_state.selectedKey.channel));
            }
            return removed;
        }

        for (int ch = 0; ch < channels; ++ch) {
            eraseAtTime(*GetCurveByChannel(tr, ch));
        }

        return removed;
    }

    bool AnimationPanel::DeleteSelectedKey() {
        if (!IsSelectedKeyValid()) {
            m_state.selectedKey = {};
            m_state.draggingKey = false;
            return false;
        }

        auto& tracks = m_loadedClip->GetTracksMutable();
        auto& tr = tracks[m_state.selectedKey.trackIndex];
        auto* c = GetCurveByChannel(tr, m_state.selectedKey.channel);

        c->keys.erase(c->keys.begin() + m_state.selectedKey.keyIndex);

        m_state.selectedKey = {};
        m_state.draggingKey = false;
        return true;
    }

    void AnimationPanel::BeginPreview() {
        if (!m_loadedClip) return;
        if (m_selectedEntity == NE::ECS::NO_ENTITY) return;

        if (m_previewActive && m_previewEntity != m_selectedEntity) {
            EndPreview();
        }
        if (m_previewActive) return;

        m_previewActive = true;
        m_previewEntity = m_selectedEntity;
        m_previewBaseline.clear();

        auto& animator = NE::ECS::Command::GetEntityAnimator(m_previewEntity);
        animator.isPlaying = false;

        std::unordered_set<BaselineKey, BaselineKeyHash> dedup;
        for (const auto& tr : m_loadedClip->GetTracks()) {
            if (tr.componentTypeId == NE::ECS::Query::GetEntityMetaComponentType())
                CaptureBaselineFromComponent<NE::ECS::Component::EntityMeta>(m_previewEntity, tr, "EntityMeta", m_previewBaseline, dedup);
            else if (tr.componentTypeId == NE::ECS::Query::GetTransformComponentType())
                CaptureBaselineFromComponent<NE::ECS::Component::Transform>(m_previewEntity, tr, "Transform", m_previewBaseline, dedup);
            else if (tr.componentTypeId == NE::ECS::Query::GetRendererComponentType())
                CaptureBaselineFromComponent<NE::ECS::Component::Renderer>(m_previewEntity, tr, "Renderer", m_previewBaseline, dedup);
            else if (tr.componentTypeId == NE::ECS::Query::GetLightComponentType())
                CaptureBaselineFromComponent<NE::ECS::Component::Light>(m_previewEntity, tr, "Light", m_previewBaseline, dedup);
        }

        ApplyPreviewPose();
    }

    void AnimationPanel::EndPreview() {
        if (!m_previewActive) return;
        if (m_previewEntity == NE::ECS::NO_ENTITY) return;

        for (const auto& be : m_previewBaseline) {
            if (be.componentTypeId == NE::ECS::Query::GetEntityMetaComponentType())
                RestoreBaselineToComponent<NE::ECS::Component::EntityMeta>(m_previewEntity, be, "EntityMeta");
            else if (be.componentTypeId == NE::ECS::Query::GetTransformComponentType())
                RestoreBaselineToComponent<NE::ECS::Component::Transform>(m_previewEntity, be, "Transform");
            else if (be.componentTypeId == NE::ECS::Query::GetRendererComponentType())
                RestoreBaselineToComponent<NE::ECS::Component::Renderer>(m_previewEntity, be, "Renderer");
            else if (be.componentTypeId == NE::ECS::Query::GetLightComponentType())
                RestoreBaselineToComponent<NE::ECS::Component::Light>(m_previewEntity, be, "Light");
        }

        auto& animator = NE::ECS::Command::GetEntityAnimator(m_previewEntity);
        animator.isPlaying = false;

        m_previewBaseline.clear();
        m_previewEntity = NE::ECS::NO_ENTITY;
        m_previewActive = false;

        m_state.playing = false;
    }

    void AnimationPanel::EnsurePreviewActiveForInteraction() {
        if (!m_loadedClip || m_selectedEntity == NE::ECS::NO_ENTITY)
            return;

        if (!m_state.preview)
            m_state.preview = true;

        if (!m_previewActive || m_previewEntity != m_selectedEntity)
            BeginPreview();
    }

    void AnimationPanel::EndPreviewIfContextInvalid() {
        if (!m_previewActive) return;

        if (!m_state.preview || !m_loadedClip || m_selectedEntity == NE::ECS::NO_ENTITY || m_previewEntity != m_selectedEntity) {
            EndPreview();
        }
    }

    void AnimationPanel::SetTimeAndApply(float t) {
        SetTime(t);
        EnsurePreviewActiveForInteraction();
        if (m_state.preview) {
            ApplyPreviewPose();
        }
    }


    void AnimationPanel::ApplyPreviewPose() {
        if (!m_previewActive || !m_loadedClip) return;
        if (m_previewEntity == NE::ECS::NO_ENTITY) return;

		NE::PreviewAnimation(m_previewEntity, *m_loadedClip, m_state.time);
    }


    void AnimationPanel::DrawLeftPanel(bool hasClip) {
        ImGui::BeginDisabled(!hasClip);

        bool prev = m_state.preview;
        if (ImGui::Checkbox("Preview", &m_state.preview)) {
            if (m_state.preview && !prev) {
                BeginPreview();
            } else if (!m_state.preview && prev) {
                m_state.playing = false;
                auto& animator = NE::ECS::Command::GetEntityAnimator(m_selectedEntity);
                animator.isPlaying = false;
                EndPreview();
            }
        }

        ImGui::SameLine();
        ImGui::Checkbox("Record", &m_state.record);

        ImGui::SameLine();
        ImGui::BeginDisabled(!m_state.record || m_state.selectedTrack < 0);
        if (ImGui::Button("Key")) {
            EnsurePreviewActiveForInteraction();
            RecordKeyForTrack(m_state.selectedTrack);
            ApplyPreviewPose();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            auto rec = Assets::AssetManager::GetInstance().GetRecordBySource(m_loadedClipPath);
            if (rec) {
                if (auto* clipAsset = dynamic_cast<Assets::AnimationClipAsset*>(rec->asset.get())) {
                    clipAsset->SaveAnimationClip(m_loadedClipPath, *m_loadedClip);
                }
            }
        }

        auto smallBtn = [&](const char* label) { return ImGui::Button(label, ImVec2(36, 0)); };

        const float L = hasClip ? m_loadedClip->GetLengthSeconds() : 0.0f;

        if (smallBtn("|<")) SetTimeAndApply(0.0f);
        ImGui::SameLine();
        if (smallBtn("<K")) { EnsurePreviewActiveForInteraction(); StepToPrevKey(); if (m_state.preview) ApplyPreviewPose(); }
        ImGui::SameLine();

        if (smallBtn(m_state.playing ? "||" : ">")) {
            EnsurePreviewActiveForInteraction();

            m_state.playing = !m_state.playing;

            auto& animator = NE::ECS::Command::GetEntityAnimator(m_selectedEntity);
            animator.isPlaying = false;
        }

        ImGui::SameLine();
        if (smallBtn("K>")) { EnsurePreviewActiveForInteraction(); StepToNextKey(); if (m_state.preview) ApplyPreviewPose(); }
        ImGui::SameLine();
        if (smallBtn(">|")) SetTimeAndApply(L);

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
        if (!hasClip) {
            float windowWidth = ImGui::GetWindowSize().x;
            float textWidth = ImGui::CalcTextSize("No clip loaded :(").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            float centeredPosX = (windowWidth - textWidth) * 0.5f;

            float windowHeight = ImGui::GetWindowSize().y;
            float textHeight = ImGui::CalcTextSize("No clip loaded :(").y + ImGui::GetStyle().FramePadding.y * 2.0f;
            float centeredPosY = (windowHeight - textHeight) * 0.5f;

            ImGui::SetCursorPos(ImVec2(centeredPosX, centeredPosY));
            ImGui::TextDisabled("No clip loaded :(");
            m_state.playing = false;
            return;
        }

        ImGuiIO& io = ImGui::GetIO();

        if (m_state.playing) {
            SetTime(m_state.time + io.DeltaTime * m_state.playbackSpeed);

            if (m_state.preview) {
                if (!m_previewActive) BeginPreview();
                ApplyPreviewPose();
            }

            if (!m_loadedClip->IsLooping() && m_state.time >= m_loadedClip->GetLengthSeconds()) {
                m_state.playing = false;
            }
        }

        if (m_state.record && m_state.selectedTrack >= 0) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                ImGui::IsKeyPressed(ImGuiKey_K)) {
                EnsurePreviewActiveForInteraction();
                RecordKeyForTrack(m_state.selectedTrack);
                ApplyPreviewPose();
            }
        }

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            !io.WantTextInput &&
            ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            EnsurePreviewActiveForInteraction();
            if (DeleteSelectedKey()) {
                ApplyPreviewPose();
            }
        }

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

            if (ImGui::IsItemActive() || (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
                float mx = io.MousePos.x;
                float t = t0 + (mx - rulerRect.Min.x) / pxPerSec;
                SetTime(t);
                EnsurePreviewActiveForInteraction();
                if (m_state.preview) ApplyPreviewPose();
            }

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

            // Track rows
            auto& tracks = m_loadedClip->GetTracksMutable();
            const float rowH = m_state.rowHeight;

            for (int ti = 0; ti < (int)tracks.size(); ++ti) {
                NE::Animation::AnimTrack& tr = tracks[ti];

                ImGui::TableNextRow(ImGuiTableRowFlags_None, rowH);

                ImGui::TableSetColumnIndex(0);
                //ImGui::TextUnformatted(tr.relativePath.c_str());
                //ImGui::SameLine();
                //ImGui::TextDisabled("%s", display.c_str());

                std::string display = GetTrackDisplayName(tr);
                std::string label = display + "##track_" + std::to_string(ti);

                if (ImGui::Selectable(label.c_str(), m_state.selectedTrack == ti, ImGuiSelectableFlags_SpanAllColumns)) {
                    m_state.selectedTrack = ti;
                }
                //ImGui::SameLine();
                //ImGui::TextDisabled("(%s)", ValueTypeName(tr.type));

                ImGui::TableSetColumnIndex(1);

                ImVec2 cellPos = ImGui::GetCursorScreenPos();
                ImVec2 cellAvail = ImGui::GetContentRegionAvail();
                ImRect rowRect(cellPos, ImVec2(cellPos.x + cellAvail.x, cellPos.y + rowH));

                ImDrawList* dl = ImGui::GetWindowDrawList();
                DrawTrackRowBackground(dl, rowRect, ti);

                DrawTrackKeys(dl, tr, ti, rowRect, pxPerSec, t0);

                float phx = rowRect.Min.x + (m_state.time - t0) * pxPerSec;
                dl->AddLine(ImVec2(phx, rowRect.Min.y), ImVec2(phx, rowRect.Max.y), IM_COL32(255, 120, 60, 140), 1.0f);

                ImGui::SetCursorScreenPos(cellPos);
                ImGui::InvisibleButton(("RowHit##" + std::to_string(ti)).c_str(), ImVec2(cellAvail.x, rowH));

                const bool rowHovered = ImGui::IsItemHovered();

                if (rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ImVec2 mouse = io.MousePos;

                    KeyRef picked = PickKeyAt(tr, ti, rowRect, pxPerSec, t0, mouse);

                    if (picked.trackIndex >= 0) {
                        EnsurePreviewActiveForInteraction();

                        m_state.selectedKey = picked;
                        m_state.selectedTrack = ti;

                        auto* c = GetCurveByChannel(tr, picked.channel);
                        const float kt = c->keys[picked.keyIndex].time;

                        SetTime(kt);
                        ApplyPreviewPose();
                        // start dragging key...
                        //m_state.selectedKeyOriginalTime = c->keys[picked.keyIndex].time;
                        //m_state.draggingKey = true;
                    } else {
                        EnsurePreviewActiveForInteraction();
                        m_state.selectedTrack = ti;
                        float t = t0 + (mouse.x - rowRect.Min.x) / pxPerSec;
                        SetTime(t);
                        m_state.selectedKey = {};
                        if (m_state.preview) ApplyPreviewPose();
                    }
                }

                if (m_state.draggingKey &&
                    m_state.selectedKey.trackIndex == ti &&
                    ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    auto& sTr = tracks[m_state.selectedKey.trackIndex];
                    auto* c = GetCurveByChannel(sTr, m_state.selectedKey.channel);

                    if (m_state.selectedKey.keyIndex >= 0 && m_state.selectedKey.keyIndex < (int)c->keys.size()) {
                        float t = t0 + (io.MousePos.x - rowRect.Min.x) / pxPerSec;
                        if (t < 0.0f) t = 0.0f;

                        if (m_state.record) {
                            float Lclip = m_loadedClip->GetLengthSeconds();
                            if (t > Lclip) m_loadedClip->SetLengthSeconds(t);
                        } else {
                            float Lclip = m_loadedClip->GetLengthSeconds();
                            t = Clamp(t, 0.0f, std::max(0.0f, Lclip));
                        }

                        c->keys[m_state.selectedKey.keyIndex].time = t;

                        std::sort(c->keys.begin(), c->keys.end(),
                            [](const NE::Animation::AnimKeyF& a, const NE::Animation::AnimKeyF& b) { return a.time < b.time; });
                    }
                }
                if (m_state.draggingKey && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    m_state.draggingKey = false;
                }

                if (rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    ImGui::OpenPopup(("KeyMenu##" + std::to_string(ti)).c_str());
                }

                if (ImGui::BeginPopup(("KeyMenu##" + std::to_string(ti)).c_str())) {
                    if (ImGui::MenuItem("Add key at playhead (all channels)")) {
                        EnsurePreviewActiveForInteraction();
                        const int chN = ChannelCount(tr.type);
                        for (int ch = 0; ch < chN; ++ch) {
                            auto* c = GetCurveByChannel(tr, ch);
                            InsertOrUpdateKey(*c, m_state.time, 0.0f);
                        }
                        ApplyPreviewPose();
                    }

                    bool hasSelectedKeyInTrack = IsSelectedKeyValid() && m_state.selectedKey.trackIndex == ti;
                    if (ImGui::MenuItem("Delete selected key", "Del", false, hasSelectedKeyInTrack)) {
                        EnsurePreviewActiveForInteraction();
                        if (DeleteSelectedKey()) {
                            ApplyPreviewPose();
                        }
                    }

                    if (ImGui::MenuItem("Delete keys at selected time (all channels)", nullptr, false, hasSelectedKeyInTrack)) {
                        const auto* selCurve = GetCurveByChannel(tr, m_state.selectedKey.channel);
                        const float selectedTime = selCurve->keys[m_state.selectedKey.keyIndex].time;

                        EnsurePreviewActiveForInteraction();
                        if (DeleteKeysAtTime(tr, selectedTime, true) > 0) {
                            m_state.selectedKey = {};
                            m_state.draggingKey = false;
                            ApplyPreviewPose();
                        }
                    }

                    if (ImGui::MenuItem("Delete track")) {
                        if (m_state.selectedTrack == ti) {
                            m_state.selectedTrack = -1;
                        } else if (m_state.selectedTrack > ti) {
                            --m_state.selectedTrack;
                        }

                        if (m_state.selectedKey.trackIndex == ti) {
                            m_state.selectedKey = {};
                            m_state.draggingKey = false;
                        } else if (m_state.selectedKey.trackIndex > ti) {
                            --m_state.selectedKey.trackIndex;
                        }

                        tracks.erase(tracks.begin() + ti);
                        ImGui::EndPopup();
                        ImGui::EndTable();
                        return;
                    }
                    ImGui::EndPopup();
                }
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

        bool ok = false;
        if (tr.componentTypeId == NE::ECS::Query::GetEntityMetaComponentType())
            ok = RecordTrackFromComponent<NE::ECS::Component::EntityMeta>(m_selectedEntity, tr, time, "EntityMeta");
        else if (tr.componentTypeId == NE::ECS::Query::GetTransformComponentType())
            ok = RecordTrackFromComponent<NE::ECS::Component::Transform>(m_selectedEntity, tr, time, "Transform");
        else if (tr.componentTypeId == NE::ECS::Query::GetRendererComponentType())
            ok = RecordTrackFromComponent<NE::ECS::Component::Renderer>(m_selectedEntity, tr, time, "Renderer");
        else if (tr.componentTypeId == NE::ECS::Query::GetLightComponentType())
            ok = RecordTrackFromComponent<NE::ECS::Component::Light>(m_selectedEntity, tr, time, "Light");

        if (ok) {
            float L = m_loadedClip->GetLengthSeconds();
            if (time > L) {
                m_loadedClip->SetLengthSeconds(time);
            }
        }

        return ok;
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

        if (keyedAny) {
            float L = m_loadedClip->GetLengthSeconds();
            if (m_state.time > L) m_loadedClip->SetLengthSeconds(m_state.time);
        }
        return keyedAny;
    }

    void AnimationPanel::DrawTrackKeys(
        ImDrawList* dl,
        NE::Animation::AnimTrack& tr,
        int ti,
        const ImRect& rowRect,
        float pxPerSec,
        float t0
    ) {
        const float y = (rowRect.Min.y + rowRect.Max.y) * 0.5f;
        const float r = 5.0f;

        const int channels = ChannelCount(tr.type);

        for (int ch = 0; ch < channels; ++ch) {
            auto* c = GetCurveByChannel(tr, ch);
            for (int ki = 0; ki < (int)c->keys.size(); ++ki) {
                const float kt = c->keys[ki].time;
                const float x = rowRect.Min.x + (kt - t0) * pxPerSec;

                if (x < rowRect.Min.x - 10.0f || x > rowRect.Max.x + 10.0f) continue;

                const bool selected =
                    (m_state.selectedKey.trackIndex == ti &&
                        m_state.selectedKey.channel == ch &&
                        m_state.selectedKey.keyIndex == ki);

                ImU32 col = selected ? IM_COL32(255, 220, 120, 255) : IM_COL32(220, 220, 220, 255);
                DrawDiamond(dl, ImVec2(x, y), r, col);
            }
        }
    }


    void AnimationPanel::MenuEntityMeta(uint32_t e) {
        DrawAnimFieldMenuForComponent<NE::ECS::Component::EntityMeta>(
            e, NE::ECS::Query::GetEntityMetaComponentType(), "EntityMeta");
    }

    void AnimationPanel::MenuTransform(uint32_t e) {
        DrawAnimFieldMenuForComponent<NE::ECS::Component::Transform>(
            e, NE::ECS::Query::GetTransformComponentType(), "Transform");
    }

    void AnimationPanel::MenuRenderer(uint32_t e) {
        DrawAnimFieldMenuForComponent<NE::ECS::Component::Renderer>(
            e, NE::ECS::Query::GetRendererComponentType(), "Renderer");
    }

    void AnimationPanel::MenuLight(uint32_t e) {
        DrawAnimFieldMenuForComponent<NE::ECS::Component::Light>(
            e, NE::ECS::Query::GetLightComponentType(), "Light");
    }

}
