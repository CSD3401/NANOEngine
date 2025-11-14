#include "AnimationPanel.hpp"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstring>                          // <-- add this

#include "../EditorScene.hpp"
#include "../EditorEntity.hpp"
#include "../EditorEvents.hpp"
#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Animator.hpp>
#include <Animation/TransformClipIO.hpp>

namespace Editor {

    using namespace NE;
    using namespace NE::ECS;
    using namespace NE::ECS::Component;

    static void SortAndUnique(std::vector<NE::Animation::KeyframeVec3>& ks) {
        std::sort(ks.begin(), ks.end(), [](auto& a, auto& b) { return a.t < b.t; });
        ks.erase(std::unique(ks.begin(), ks.end(),
            [](auto& a, auto& b) { return std::abs(a.t - b.t) < 1e-4f; }),
            ks.end());
    }

    void AnimationPanel::OnImGuiRender()
    {
        if (ImGui::Begin("Animation")) {
            uint32_t selected = 0;
            if (EditorScene::s_selectedEntity)
                selected = EditorScene::s_selectedEntity->linkedEntity;

            Animator* animatorPtr = nullptr;
            Transform* trPtr = nullptr;

            if (selected != 0) {
                trPtr = &ECS::Command::GetEntityTransform(selected);
                if (ECS::Query::HasAnimator(selected)) {
                    animatorPtr = &ECS::Command::GetEntityAnimator(selected);
                }
            }

            if (selected == 0 || !trPtr) {
                ImGui::TextDisabled("Select an entity with a Transform to animate.");
                ImGui::End();
                return;
            }

            // Ensure clip for this entity
            auto& clipPtr = m_entityClips[selected];
            if (!clipPtr) clipPtr = std::make_shared<NE::Animation::TransformClip>();

            // header now gets the clip by reference
            DrawHeader(animatorPtr, *clipPtr);

            ImGui::Separator();
            DrawDopesheet(selected, *trPtr, *clipPtr);

            // Transport
            ImGui::Separator();
            if (ImGui::Button(m_playing ? "Pause" : "Play")) m_playing = !m_playing;
            ImGui::SameLine();
            if (ImGui::Button("Stop")) { m_playing = false; m_currentTime = 0.0f; }
            ImGui::SameLine();
            ImGui::Checkbox("Loop", &m_loop);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::DragFloat("Length (s)", &m_length, 0.01f, 0.1f, 120.0f, "%.2f");
            clipPtr->length = m_length;

            if (m_playing) {
                m_currentTime += ImGui::GetIO().DeltaTime;
                if (m_loop && m_length > 0.0f) m_currentTime = std::fmod(m_currentTime, m_length);
                if (!m_loop && m_currentTime > m_length) { m_playing = false; m_currentTime = m_length; }
                clipPtr->ApplyTo(*trPtr, m_currentTime, m_loop);
            }
        }
        ImGui::End();
    }

    void AnimationPanel::DrawHeader(Animator* animator, NEAnim::TransformClip& clip)
    {
        ImGui::TextUnformatted("Clip:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(220);

        static char buf[128] = "NewClip";
        ImGui::InputText("##clipname", buf, sizeof(buf));
        ImGui::SameLine();
        if (ImGui::Button("New")) {
#ifdef _MSC_VER
            strcpy_s(buf, "NewClip");
#else
            std::strcpy(buf, "NewClip");
#endif
            m_currentTime = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            const char* kDir = "Assets/Animations";
            std::filesystem::create_directories(kDir);
            std::string name = (std::strlen(buf) ? buf : "NewClip");
            std::string file = std::string(kDir) + "/" + name + ".neclip";

            NE::Animation::SaveTransformClip(clip, file);

            if (animator) {
                animator->activeClip = file;
                animator->playOnStart = true; // optional
            }
        }

        if (animator) {
            ImGui::Separator();
            ImGui::TextDisabled("Animator (Component)");
            ImGui::Checkbox("Play On Start", &animator->playOnStart);
            ImGui::SameLine();
            ImGui::Checkbox("Loop##anim", &animator->loop);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::DragFloat("Speed##anim", &animator->speed, 0.01f, 0.1f, 5.0f, "%.2f");
        }
    }
    void AnimationPanel::DrawDopesheet(uint32_t /*entityId*/, Transform& tr, NEAnim::TransformClip& clip)
    {
        const float rowH = 22.0f;
        const float headerW = 160.0f;
        const float timelineH = rowH * 3.0f + 8.0f;
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float timelineW = avail.x - headerW;
        const float pxPerSec = 100.0f * m_zoom;
        const float height = timelineH + 26.0f;

        ImGui::BeginChild("anim_left", ImVec2(headerW, height), ImGuiChildFlags_Border);
        ImGui::TextUnformatted("Transform");
        ImGui::Separator();
        ImGui::TextUnformatted("Position");
        ImGui::TextUnformatted("Rotation");
        ImGui::TextUnformatted("Scale");
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("anim_timeline", ImVec2(timelineW, height), ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);

        // Timeline grid
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImVec2 end = ImVec2(origin.x + timelineW, origin.y + timelineH);

        // Ruler
        float secVisible = std::max(1.0f, timelineW / pxPerSec);
        for (int i = 0; i <= int(std::ceil(secVisible)); ++i) {
            float x = origin.x + i * pxPerSec;
            dl->AddLine(ImVec2(x, origin.y), ImVec2(x, end.y), IM_COL32(60, 60, 60, 255));
            char label[32]; snprintf(label, 32, "%ds", i);
            dl->AddText(ImVec2(x + 2, origin.y + 2), IM_COL32_WHITE, label);
        }
        // Row separators
        float y0 = origin.y + rowH + 4.0f;
        dl->AddLine(ImVec2(origin.x, y0), ImVec2(end.x, y0), IM_COL32(90, 90, 90, 255));
        float y1 = y0 + rowH;
        dl->AddLine(ImVec2(origin.x, y1), ImVec2(end.x, y1), IM_COL32(90, 90, 90, 255));

        // Draw keys helper
        auto drawKeys = [&](const std::vector<NEAnim::KeyframeVec3>& keys, float yRow) {
            for (auto& k : keys) {
                float x = origin.x + k.t * pxPerSec;
                ImVec2 p = ImVec2(x, yRow + rowH * 0.5f);
                dl->AddTriangleFilled(ImVec2(p.x - 4, p.y - 4), ImVec2(p.x + 4, p.y - 4), ImVec2(p.x, p.y + 4), IM_COL32(255, 200, 0, 255));
            }
            };
        drawKeys(clip.pos, origin.y);
        drawKeys(clip.rot, origin.y + rowH + 4.0f);
        drawKeys(clip.scl, origin.y + 2 * rowH + 4.0f);

        // Scrubber
        float scrubX = origin.x + m_currentTime * pxPerSec;
        dl->AddLine(ImVec2(scrubX, origin.y), ImVec2(scrubX, end.y), IM_COL32(200, 50, 50, 255), 2.0f);

        // Mouse interaction
        ImGui::InvisibleButton("timeline_ib", ImVec2(timelineW, timelineH));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            float dx = ImGui::GetIO().MouseDelta.x;
            m_currentTime = std::clamp(m_currentTime + dx / pxPerSec, 0.0f, std::max(clip.length, 0.001f));
            clip.ApplyTo(tr, m_currentTime, m_loop);
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
            float localX = ImGui::GetIO().MousePos.x - origin.x;
            m_currentTime = std::clamp(localX / pxPerSec, 0.0f, std::max(clip.length, 0.001f));
            clip.ApplyTo(tr, m_currentTime, m_loop);
        }

        // Keyframe ops
        if (ImGui::Button("Add Key (P)")) { AddKey(clip, m_currentTime, tr, true, false, false); }
        ImGui::SameLine();
        if (ImGui::Button("Add Key (R)")) { AddKey(clip, m_currentTime, tr, false, true, false); }
        ImGui::SameLine();
        if (ImGui::Button("Add Key (S)")) { AddKey(clip, m_currentTime, tr, false, false, true); }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        ImGui::DragFloat("Zoom", &m_zoom, 0.01f, 0.2f, 4.0f, "%.2f");

        ImGui::EndChild();
    }

    void AnimationPanel::AddKey(NEAnim::TransformClip& clip, float t, const Transform& tr, bool pos, bool rot, bool scl)
    {
        if (pos) { clip.pos.push_back({t, tr.position}); SortAndUnique(clip.pos); }
        if (rot) { clip.rot.push_back({t, tr.rotation}); SortAndUnique(clip.rot); }
        if (scl) { clip.scl.push_back({t, tr.scale});    SortAndUnique(clip.scl); }
        clip.length = std::max(clip.length, t);
    }

} // namespace Editor
