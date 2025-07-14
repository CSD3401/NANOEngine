#include "InspectorPanel.hpp"
#include <imgui/imgui.h>
#include <ECSInternals.hpp>
#include "ECS/Core/Signature.hpp"
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Light.hpp>
#include <Core/Reflection.hpp>
#include <Math/Vec3.hpp>
#include "../EditorScene.hpp"
#include <Engine.hpp>
#include <imgui/widgets/imsearch/imsearch.h>
#include "../EditorUI.hpp"

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
            //return ImGui::DragFloat3(desc.name.data(), value.Data(), 0.1f);
			return Editor::DrawVec3Control(desc.name.data(), value, 0.0f, 75.0f);
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
                    //char buf[256]; 
                    //strncpy_s(buf, comp.modelPath.string().c_str(), sizeof(buf));
                    //ImGui::InputText("Model", buf, sizeof(buf));
                    //comp.modelPath = buf;
                    bool openPopup = false;
                    DrawAssetField("Model", comp.modelPath.string(), "+", 0.f, &openPopup);
                    if (openPopup) {
                        ImGui::OpenPopup("AssetPicker_Model");
					}

                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            std::string dropped((const char*)p->Data, p->DataSize - 1);

							if (comp.materialPath.empty()) { // If material is not set, assign a default one
                                AssignRendererMaterial(comp, "Assets/Basic.nanomat");
							} // done for rapid prototyping, should be removed later

                            AssignRendererModel(comp, dropped);
                        }
                        ImGui::EndDragDropTarget();
                    }

                    static std::string searchQuery;
                    if (ImGui::BeginPopup("AssetPicker_Model")) {
                        ImGui::Text("Select a Model");
                        ImGui::Separator();
                        auto& assets = NANOEngine::GetAllModels();

                        if (ImSearch::BeginSearch()) {
                            ImSearch::SearchBar();

                            for (const auto& [name, asset] : assets) {
                                ImSearch::SearchableItem(name.c_str(),
                                    [name, &comp](const char*) {
                                        if (ImGui::Selectable(name.c_str())) {
                                            AssignRendererModel(comp, name);
                                            AssignRendererMaterial(comp, "Assets/Basic.nanomat");
                                            ImGui::CloseCurrentPopup();
                                        }
									});
                            }

                            ImSearch::EndSearch();
                        }
                        ImGui::EndPopup();
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
					m_loadedMaterial = GetMaterial(EditorScene::selectedMaterial);
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
                    if (Editor::DrawFloatControl(name.c_str(), v, 0.1f)) {
                        m_loadedMaterial->SetUniformFloat(name, v);
                    }
                }

                for (auto& [name, val] : m_loadedMaterial->GetVec3Uniforms()) {
                    NANOEngine::Math::Vec3 v = val;
                    if (Editor::DrawVec3Control(name.c_str(), v, 0.0f, 100.0f)) {
                        m_loadedMaterial->SetUniformVec3(name, v);
                    }
                }

                for (auto& [name, val] : m_loadedMaterial->GetIntUniforms()) {
                    int i = val;
					Editor::DrawIntControl(name.c_str(), i);
                    if (ImGui::DragInt(name.c_str(), &i)) {
                        m_loadedMaterial->SetUniformInt(name, i);
                    }
                }
            }
        }

        ImGui::End();
    }
}
