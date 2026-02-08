#include "../Panels/AnimationRuntimePanel.hpp"
#include <imgui/imgui.h>
#include <cstdio>          // std::snprintf
#include <filesystem>      // std::filesystem::is_regular_file

#include "../EditorScene.hpp"
#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/Animator.hpp>
#include <Animation/AnimatorController.hpp>
#include <Animation/AnimatorControllerIO.hpp>

namespace Editor {

    using namespace NE;
    using namespace NE::ECS;
    using namespace NE::ECS::Component;
    void AnimatorRuntimePanel::OnImGuiRender() {
        ImGui::Begin("Animator (Runtime)");

        const uint32_t eid = EditorScene::s_selection.GetPrimary();
        if (eid == NE::ECS::NO_ENTITY) {
            ImGui::TextDisabled("Select an entity.");
            ImGui::End();
            return;
        }

        if (!ECS::Query::HasAnimator(eid)) {
            ImGui::TextDisabled("This entity has no Animator component.");
            if (ImGui::Button("Add Animator")) {
                NE::ECS::Command::AddAnimatorComponent(eid);
            }
            ImGui::End();
            return;
        }

        // Safe to fetch; component exists
        NE::ECS::Component::Animator& anim = NE::ECS::Command::GetEntityAnimator(eid);

        // Basic runtime toggles
        ImGui::Checkbox("Play On Start", &anim.playOnStart);
        ImGui::Checkbox("Loop", &anim.loop);
        ImGui::DragFloat("Speed", &anim.speed, 0.01f, 0.0f, 5.0f);

        // Controller path edit (commit when user finishes editing)
        {
            char buf[512];
            std::snprintf(buf, sizeof(buf), "%s", anim.controllerPath.c_str());
            bool edited = ImGui::InputText("Controller", buf, IM_ARRAYSIZE(buf));
            bool committed = edited && ImGui::IsItemDeactivatedAfterEdit();
            if (committed) {
                anim.controllerPath = buf;
            }
        }

        if (anim.controllerPath.empty()) {
            ImGui::TextDisabled("Assign a controller to edit parameters.");
            ImGui::End();
            return;
        }

        // Hot-reload cache for this panel
        static NE::Animation::AnimatorController sCtrl;
        static std::string sPath;
        static std::filesystem::file_time_type sTime{}; // last write time we loaded

        bool pathChanged = (sPath != anim.controllerPath);
        if (pathChanged) {
            sPath.clear();
            sCtrl = NE::Animation::AnimatorController{};
            sTime = {};
            sPath = anim.controllerPath;
        }

        // Reload if path changed or file timestamp changed
        if (!sPath.empty() && std::filesystem::exists(sPath) && std::filesystem::is_regular_file(sPath)) {
            auto now = std::filesystem::last_write_time(sPath);
            if (pathChanged || sTime != now) {
                NE::Animation::AnimatorController tmp;
                if (NE::Animation::LoadAnimatorController(tmp, sPath)) {
                    sCtrl = std::move(tmp);
                    sTime = now;
                }
                // else: keep previous valid sCtrl; UI will show "no states" if empty
            }
        }

        if (sCtrl.states.empty()) {
            ImGui::TextDisabled("Controller has no states or failed to load.");
            ImGui::End();
            return;
        }

        // Parameters UI (drives runtime overrides)
        if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            using NE::Animation::ParamType;
            for (const auto& p : sCtrl.parameters) {
                ImGui::PushID(p.name.c_str());
                switch (p.type) {
                case ParamType::Bool: {
                    bool v = anim.bools.count(p.name) ? anim.bools[p.name] : p.b;
                    if (ImGui::Checkbox(("Param: " + p.name + "##param").c_str(), &v))
                        anim.bools[p.name] = v;
                } break;
                case ParamType::Float: {
                    float v = anim.floats.count(p.name) ? anim.floats[p.name] : p.f;
                    if (ImGui::DragFloat(("Param: " + p.name + "##param").c_str(), &v, 0.01f))
                        anim.floats[p.name] = v;
                } break;
                case ParamType::Int: {
                    int v = anim.ints.count(p.name) ? anim.ints[p.name] : p.i;
                    if (ImGui::DragInt(("Param: " + p.name + "##param").c_str(), &v, 1))
                        anim.ints[p.name] = v;
                } break;
                case ParamType::Trigger: {
                    if (ImGui::Button(("Param: " + p.name + "##param").c_str()))
                        anim.setTriggers.push_back(p.name);
                } break;
                }
                ImGui::PopID();
            }
        }

        ImGui::End();
    }
} // namespace Editor
