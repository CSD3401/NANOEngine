#include "InspectorPanel.hpp"
#include <imgui/imgui.h>
#include <ECSInternals.hpp>
#include "src/ECS/Core/Signature.hpp"
#include <src/ECS/Components/Transform.hpp>
#include <src/ECS/Components/Renderer.hpp>
#include <src/ECS/Components/Light.hpp>
#include <src/Core/Reflection.hpp>
#include <src/Math/Vec3.hpp>
#include "../EditorScene.hpp"
#include <Engine.hpp>

namespace {
    template<typename Owner, typename T>
    bool DrawField(const NANOEngine::Core::FieldDescriptor<Owner, T>& desc, T& value) {
        using namespace NANOEngine;
        if constexpr (std::is_same_v<T, bool>) {
            return ImGui::Checkbox(desc.name.data(), &value);
        } else if constexpr (std::is_same_v<T, int>) {
            return ImGui::DragInt(desc.name.data(), &value);
        } else if constexpr (std::is_same_v<T, float>) {
            return ImGui::DragFloat(desc.name.data(), &value, 0.1f);
        } else if constexpr (std::is_same_v<T, Math::Vec3>) {
            return ImGui::DragFloat3(desc.name.data(), value.Data(), 0.1f);
        } 
        //else if constexpr (std::is_same_v<T, enum>) {

        //} 
        else {
            ImGui::Text("%s (unsupported)", desc.name.data());
            return false;
        }
    }
}

namespace Editor {
    InspectorPanel::InspectorPanel() {
        m_loadedMaterial = nullptr;
        m_loadedPath = "";
    }

    void InspectorPanel::OnImGuiRender()
    {
        ImGui::Begin("Inspector", nullptr,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImVec2 panelPos = ImGui::GetCursorScreenPos();
        ImVec2 panelSize = ImGui::GetContentRegionAvail();

        if (EditorScene::s_selectedEntity) {
            using namespace NANOEngine;
            uint32_t entity = EditorScene::s_selectedEntity->linkedEntity;

            ECS::Signature sig(GetEntitySignature(entity));
            for (const auto& [typeIdx, compType] : GetRegisteredComponentTypes()) {
                if (!sig.test(compType)) continue;

                if (typeIdx == typeid(ECS::Component::Transform)) {
                    auto& comp = GetEntityTransform(entity);
                    ImGui::SeparatorText("Transform");
                    Core::ForEachField<ECS::Component::Transform>(comp, [&](auto&& desc, auto& field) {
                        comp.isDirty |= DrawField(desc, field);
                        });
                } else if (typeIdx == typeid(ECS::Component::Renderer)) {
                    auto& comp = GetEntityRenderer(entity);
                    ImGui::SeparatorText("Renderer");
                    char buf[256]; strncpy_s(buf, comp.modelPath.string().c_str(), sizeof(buf));
                    ImGui::InputText("Model", buf, sizeof(buf));
                    comp.modelPath = buf;
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            std::string dropped((const char*)p->Data, p->DataSize - 1);
                            AssignRendererModel(comp, dropped);
                        }
                        ImGui::EndDragDropTarget();
                    }

                    char bufMat[256]; 
                    strncpy_s(bufMat, comp.materialPath.string().c_str(), sizeof(bufMat));
                    ImGui::InputText("Material", bufMat, sizeof(bufMat));
                    comp.materialPath = bufMat;
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("MATERIAL_PATH")) {
                            std::string dropped((const char*)p->Data, p->DataSize - 1);
                            AssignRendererMaterial(comp, dropped);
                        }
                        ImGui::EndDragDropTarget();
                    }
                } else if (typeIdx == typeid(ECS::Component::Light)) {
                    auto& comp = GetEntityLight(entity);
                    ImGui::SeparatorText("Light");

                    static const char* LightTypeNames[] = { "Directional", "Point", "Spot" };
                    int currentType = static_cast<int>(comp.type);
                    if (ImGui::Combo("Type", &currentType, LightTypeNames, IM_ARRAYSIZE(LightTypeNames))) {
                        comp.type = static_cast<ECS::Component::Light::Type>(currentType);
                    }

                    Core::ForEachField<ECS::Component::Light>(comp, [](auto&& desc, auto& field) {
                        DrawField(desc, field);
                        });
                }
            }

            //if (EditorScene::s_selectedEntity) {
            //    using namespace NANOEngine;
            //    uint32_t entity = EditorScene::s_selectedEntity->linkedEntity;
            //    ECS::Signature signature(GetEntitySignature(entity));

            //    //auto& ecs = GetScene().GetECSCoordinator();
            //    for (const auto& [typeIdx, compType] : GetECSCoordinator().GetRegisteredComponentTypes()) {
            //        if (!signature.test(compType))
            //            continue;

            //        if (typeIdx == typeid(ECS::Transform)) {
            //            auto& comp = GetECSCoordinator().GetComponent<ECS::Transform>(entity);
            //            ImGui::SeparatorText("Transform");
            //            Core::ForEachField<ECS::Transform>(comp, [](auto&& desc, auto& field) {
            //                DrawField(desc, field);
            //                });
            //        } else if (typeIdx == typeid(ECS::Renderer)) {
            //            auto& comp = GetECSCoordinator().GetComponent<ECS::Renderer>(entity);
            //            ImGui::SeparatorText("Renderer");
            //            Core::ForEachField<ECS::Renderer>(comp, [](auto&& desc, auto& field) {
            //                DrawField(desc, field);
            //                });
            //        }
            //    }
            //}
            if (ImGui::Button("Add Component")) {
                ImGui::OpenPopup("ComponentList");
            }

            if (ImGui::BeginPopup("ComponentList")) {
                if (ImGui::MenuItem("Light")) {
                    AddLightComponent(EditorScene::s_selectedEntity->linkedEntity);
                }
                ImGui::EndPopup();
            }
        } else if (EditorScene::selectedMaterial != "") {
            using namespace NANOEngine;
            if (m_loadedPath != EditorScene::selectedMaterial) {
                try {
                    m_loadedMaterial = Graphics::Material::LoadMaterial(EditorScene::selectedMaterial);
                    m_loadedPath = EditorScene::selectedMaterial;
                } catch (...) {
                    m_loadedMaterial.reset();
                    m_loadedPath.clear();
                }
            }

            if (m_loadedMaterial) {
                ImGui::SeparatorText("Material Uniforms");

                for (auto& [name, val] : m_loadedMaterial->GetFloatUniforms()) {
                    float v = val;
                    if (ImGui::DragFloat(name.c_str(), &v, 0.1f)) {
                        m_loadedMaterial->SetUniformFloat(name, v);
                        m_loadedMaterial->SaveMaterial(m_loadedPath);
                    }
                }

                for (auto& [name, val] : m_loadedMaterial->GetVec3Uniforms()) {
                    NANOEngine::Math::Vec3 v = val;
                    if (ImGui::DragFloat3(name.c_str(), v.Data(), 0.1f)) {
                        m_loadedMaterial->SetUniformVec3(name, v);
                        m_loadedMaterial->SaveMaterial(m_loadedPath);
                    }
                }

                for (auto& [name, val] : m_loadedMaterial->GetIntUniforms()) {
                    int i = val;
                    if (ImGui::DragInt(name.c_str(), &i)) {
                        m_loadedMaterial->SetUniformInt(name, i);
                        m_loadedMaterial->SaveMaterial(m_loadedPath);
                    }
                }
            }
        }

        ImGui::End();
    }
}
