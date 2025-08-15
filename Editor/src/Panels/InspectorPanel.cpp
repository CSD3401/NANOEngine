#include "InspectorPanel.hpp"
#include <imgui/imgui.h>
//#include <ECSInternals.hpp>
#include <EditorInterface/ECSExports.hpp>
#include "ECS/Core/Signature.hpp"
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Light.hpp>
#include <ECS/Components/Rigidbody.hpp>
#include <ECS/Components/Collider.hpp>
#include <Core/Reflection.hpp>
#include <Math/Vec3.hpp>
#include "../EditorScene.hpp"
#include <Engine.hpp>
#include <imgui/widgets/imsearch/imsearch.h>
#include "../EditorUI.hpp"

namespace {
    template<typename Owner, typename T>
    bool DrawField(const NE::Core::FieldDescriptor<Owner, T>& desc, const T&) {
        
        if constexpr (std::is_same_v<T, bool>) {
            //return ImGui::Checkbox(desc.name.data(), &value);
            return false;
        } else if constexpr (std::is_same_v<T, int>) {
            //return ImGui::DragInt(desc.name.data(), &value);
            return false;
        } else if constexpr (std::is_same_v<T, float>) {
            //return ImGui::DragFloat(desc.name.data(), &value, 0.1f);
            return false;
        } else if constexpr (std::is_same_v<T, NE::Math::Vec3>) {
            return false;
            //return ImGui::DragFloat3(desc.name.data(), value.Data(), 0.1f);
			//return Editor::DrawVec3Control(desc.name.data(), value, 0.0f, 75.0f);
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
    std::unordered_map<std::type_index, uint8_t> componentTypeRegistry;

    InspectorPanel::InspectorPanel() {
        m_loadedMaterial = nullptr;
        m_loadedPath = "";

        componentTypeRegistry = NE::ECS::Query::GetRegisteredComponentTypes();
    }

    void InspectorPanel::OnImGuiRender()
    {
        ImGui::Begin("Inspector", nullptr,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImVec2 panelPos = ImGui::GetCursorScreenPos();
        ImVec2 panelSize = ImGui::GetContentRegionAvail();

        if (EditorScene::s_selectedEntity) {
            uint32_t entity = EditorScene::s_selectedEntity->linkedEntity;

            NE::ECS::Signature sig(NE::ECS::Query::GetEntitySignature(entity));
            for (const auto& [typeIdx, compType] : componentTypeRegistry) {
                if (!sig.test(compType)) continue;

                if (typeIdx == typeid(NE::ECS::Component::Transform)) {
                    auto& comp = NE::ECS::Query::GetEntityTransform(entity);
                    ImGui::SeparatorText("Transform");
                    NE::Core::ForEachField<NE::ECS::Component::Transform>(comp, [&](auto&& , auto& ) {
                        //comp.isDirty |= DrawField(desc, field);
                        });
                } else if (typeIdx == typeid(NE::ECS::Component::Renderer)) {
                    auto& comp = NE::ECS::Query::GetEntityRenderer(entity);
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
                                //AssignRendererMaterial(comp, "Assets/Basic.nanomat");
							} // done for rapid prototyping, should be removed later

                            //AssignRendererModel(comp, dropped);
                        }
                        ImGui::EndDragDropTarget();
                    }

         //           static std::string searchQuery;
         //           if (ImGui::BeginPopup("AssetPicker_Model")) {
         //               ImGui::Text("Select a Model");
         //               ImGui::Separator();
         //               auto& assets = NANOEngine::GetAllModels();

         //               if (ImSearch::BeginSearch()) {
         //                   ImSearch::SearchBar();

         //                   for (const auto& [name, asset] : assets) {
         //                       ImSearch::SearchableItem(name.c_str(),
         //                           [name, &comp](const char*) {
         //                               if (ImGui::Selectable(name.c_str())) {
         //                                   AssignRendererModel(comp, name);
         //                                   AssignRendererMaterial(comp, "Assets/Basic.nanomat");
         //                                   ImGui::CloseCurrentPopup();
         //                               }
									//});
         //                   }

         //                   ImSearch::EndSearch();
         //               }
         //               ImGui::EndPopup();
         //           }

                    char bufMat[256]; 
                    strncpy_s(bufMat, comp.materialPath.string().c_str(), sizeof(bufMat));
                    ImGui::InputText("Material", bufMat, sizeof(bufMat));

                    //comp.materialPath = bufMat;
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("MATERIAL_PATH")) {
                            std::string dropped((const char*)p->Data, p->DataSize - 1);
                            //AssignRendererMaterial(comp, dropped);
                        }
                        ImGui::EndDragDropTarget();
                    }
                } else if (typeIdx == typeid(NE::ECS::Component::Light)) {
                    auto& comp = NE::ECS::Query::GetEntityLight(entity);
                    ImGui::SeparatorText("Light");

                    static const char* LightTypeNames[] = { "Directional", "Point", "Spot" };
                    int currentType = static_cast<int>(comp.type);
                    if (ImGui::Combo("Type", &currentType, LightTypeNames, IM_ARRAYSIZE(LightTypeNames))) {
                        //comp.type = static_cast<NE::ECS::Component::Light::Type>(currentType);
                    }

                    NE::Core::ForEachField<NE::ECS::Component::Light>(comp, [](auto&& desc, auto& field) {
                        DrawField(desc, field);
                        });
                } else if (typeIdx == typeid(NE::ECS::Component::Collider)) {
                    auto& comp = NE::ECS::Query::GetEntityCollider(entity);
                    ImGui::SeparatorText("Collider");
                    NE::Core::ForEachField<NE::ECS::Component::Collider>(comp, [&](auto&& desc, auto& field) {
                        DrawField(desc, field);
                        //comp.isDirty |= DrawField(desc, field);
                        });
                } else if (typeIdx == typeid(NE::ECS::Component::Rigidbody)) {
                    auto& comp = NE::ECS::Query::GetEntityRigidbody(entity);
                    ImGui::SeparatorText("Rigidbody");
                    NE::Core::ForEachField<NE::ECS::Component::Rigidbody>(comp, [&](auto&& desc, auto& field) {
                        if (DrawField(desc, field) && desc.name == "isStatic") {
							//NE::SetMotionType(comp.bodyID, 0U);
                        }
                        //comp.isDirty |= DrawField(desc, field);
                        });
                } 
            }

            if (ImGui::Button("Add Component")) {
                ImGui::OpenPopup("ComponentList");
            }

            if (ImGui::BeginPopup("ComponentList")) { // automate this next time with a registry
                if (ImGui::MenuItem("Renderer")) {
                    NE::ECS::Command::AddRendererComponent(EditorScene::s_selectedEntity->linkedEntity);
                }
                if (ImGui::MenuItem("Rigidbody")) {
                    NE::ECS::Command::AddColliderComponent(EditorScene::s_selectedEntity->linkedEntity);
                    NE::ECS::Command::AddRigidbodyComponent(EditorScene::s_selectedEntity->linkedEntity);
                }
                if (ImGui::MenuItem("Collider")) {
                    NE::ECS::Command::AddColliderComponent(EditorScene::s_selectedEntity->linkedEntity);
                }
                if (ImGui::MenuItem("Light")) {
                    NE::ECS::Command::AddLightComponent(EditorScene::s_selectedEntity->linkedEntity);
                }
                ImGui::EndPopup();
            }

        } else if (EditorScene::selectedMaterial != "") {
            if (m_loadedPath != EditorScene::selectedMaterial) {
                try {
					//m_loadedMaterial = GetMaterial(EditorScene::selectedMaterial);
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
                    NE::Math::Vec3 v = val;
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
