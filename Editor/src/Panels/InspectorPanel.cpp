#include "InspectorPanel.hpp"
#include <imgui/imgui.h>
#include <EditorInterface/ECSExports.hpp>
#include "ECS/Core/Signature.hpp"
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Light.hpp>
#include <ECS/Components/Rigidbody.hpp>
#include <ECS/Components/Collider.hpp>
#include <ECS/Components/EntityMeta.hpp>
#include <Core/Reflection.hpp>
#include <Math/Vec3.hpp>
#include "../EditorScene.hpp"
#include <Engine.hpp>
#include <imgui/widgets/imsearch/imsearch.h>
#include "../EditorUI.hpp"
#include "../Command/EditorSetFieldCommand.hpp"
#include "../Command/CommandHistory.hpp"
#include <unordered_map>
#include <typeinfo>
#include <bit>      // std::bit_cast
#include <array>
#include <cstddef>  // std::byte
#include <cstdint>

namespace {
    template<typename Owner, typename T>
    bool DrawField(const NE::Core::FieldDescriptor<Owner, T>& desc, T& value) {
        if constexpr (std::is_same_v<T, bool>) {
            return ImGui::Checkbox(desc.name.data(), &value);
        } else if constexpr (std::is_same_v<T, int>) {
            return ImGui::DragInt(desc.name.data(), &value);
        } else if constexpr (std::is_same_v<T, float>) {
            return ImGui::DragFloat(desc.name.data(), &value, 0.1f);
        } else if constexpr (std::is_same_v<T, NE::Math::Vec3>) {
            ImGui::BeginGroup();
            bool changed = Editor::DrawVec3Control(desc.name.data(), value, 0.0f, 75.0f);
            ImGui::EndGroup();
            return changed;
        } else {
            ImGui::Text("%s (unsupported)", desc.name.data());
            return false;
        }
    }

    template <typename Owner, typename T>
    static void SubmitSetFieldCommand(uint32_t entity,
        const NE::Core::FieldDescriptor<Owner, T>& desc,
        const T& before,
        const T& after) {
        using Cmd = Editor::SetFieldCommand<Owner, T>;

        auto getter = [=](uint32_t e) -> Owner& {
            if constexpr (std::is_same_v<Owner, NE::ECS::Component::Transform>) {
                return NE::ECS::Command::GetEntityTransform(e);
            } else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Collider>) {
                return NE::ECS::Command::GetEntityCollider(e);
            } else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Rigidbody>) {
                return NE::ECS::Command::GetEntityRigidbody(e);
            } else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Renderer>) {
                return NE::ECS::Command::GetEntityRenderer(e);
            } else if constexpr (std::is_same_v<Owner, NE::ECS::Component::Light>) {
                return NE::ECS::Command::GetEntityLight(e);
            } else {
                static_assert(sizeof(Owner) == 0, "No getter defined for this component type.");
            }
        };

        auto cmd = std::make_unique<Cmd>(entity,
            std::string(desc.name),
            desc.member,
            before,
            after,
            getter);

        Editor::CommandHistory::GetInstance().ExecuteCommand(std::move(cmd));
    }

    template <class Owner, class T>
    struct MemberPointerHasher {
        size_t operator()(T Owner::* mp) const noexcept {
            auto bytes = std::bit_cast<std::array<std::byte, sizeof(mp)>>(mp);
            size_t h = 1469598103934665603ull;
            for (std::byte b : bytes) {
                h ^= static_cast<unsigned char>(b);
                h *= 1099511628211ull;
            }
            return h;
        }
    };

    struct FieldKey {
        uint32_t entity;
        const std::type_info* ownerType;
        size_t memberId;  // hashed member pointer

        bool operator==(const FieldKey& o) const noexcept {
            return entity == o.entity && ownerType == o.ownerType && memberId == o.memberId;
        }
    };

    struct FieldKeyHash {
        size_t operator()(const FieldKey& k) const noexcept {
            size_t h = std::hash<uint32_t>{}(k.entity);
            h ^= std::hash<const void*>{}(k.ownerType) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= k.memberId + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    template<class T>
    static bool Equal(const T& a, const T& b) {
        if constexpr (std::is_floating_point_v<T>) {
            return std::fabs(a - b) <= 1e-6f;
        } else {
            return a == b;
        }
    }

}

namespace Editor {
    std::unordered_map<std::type_index, uint8_t> componentTypeRegistry;

    static std::unordered_map<FieldKey,
        std::unique_ptr<ICommand>,
        FieldKeyHash> g_activeCommands;

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

            bool isActive = true;
            if (ImGui::Checkbox("##", &isActive)) {

            }
            ImGui::SameLine();

            {
                using Owner = NE::ECS::Component::EntityMeta;
                using FieldT = std::string;

                const auto& metaRO = NE::ECS::Query::GetEntityMeta(entity);

                FieldKey nameKey{
                    entity,
                    &typeid(Owner),
                    MemberPointerHasher<Owner, FieldT>{}(&Owner::name)
                };

                std::string currentText;
                if (auto it = g_activeCommands.find(nameKey); it != g_activeCommands.end()) {
                    if (auto* live = dynamic_cast<Editor::SetFieldCommand<Owner, FieldT>*>(it->second.get())) {
                        currentText = live->After();
                    }
                }
                if (currentText.empty()) currentText = metaRO.name;

                std::string edited = currentText;

                ImGui::PushID("EntityName");
                bool changed = ImGui::InputText("##Name", edited.data(),
                    ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
                bool activated = ImGui::IsItemActivated();
                bool active = ImGui::IsItemActive();
                bool deactivated = ImGui::IsItemDeactivatedAfterEdit();
                ImGui::PopID();

                if (activated && !g_activeCommands.contains(nameKey)) {
                    using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
                    auto cmd = std::make_unique<Cmd>(
                        entity,
                        std::string("Rename Entity"),
                        &Owner::name,
                        metaRO.name,
                        metaRO.name,
                        &NE::ECS::Command::GetEntityMeta
                    );
                    g_activeCommands[nameKey] = std::move(cmd);
                }

                // Safety net: if the Activated frame was missed but we're changing, create it now
                if ((active && changed) && !g_activeCommands.contains(nameKey)) {
                    using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
                    auto cmd = std::make_unique<Cmd>(
                        entity, std::string("Rename Entity"),
                        &Owner::name, metaRO.name, metaRO.name,
                        &NE::ECS::Command::GetEntityMeta);
                    g_activeCommands[nameKey] = std::move(cmd);
                }

                // During edit: coalesce by updating After() and applying immediately
                if (active && changed) {
                    auto it = g_activeCommands.find(nameKey);
                    if (it != g_activeCommands.end()) {
                        using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
                        Cmd tmp(
                            entity, std::string{}, &Owner::name,
                            metaRO.name,
                            edited,
                            &NE::ECS::Command::GetEntityMeta
                        );
                        it->second->CoalesceFrom(tmp);
                    }
                }

                if (deactivated) {
                    auto it = g_activeCommands.find(nameKey);
                    if (it != g_activeCommands.end()) {
                        if (auto* c = dynamic_cast<Editor::SetFieldCommand<Owner, FieldT>*>(it->second.get())) {
                            if (c->Before() == c->After()) {
                                g_activeCommands.erase(it);
                                return;
                            }
                        }
                        Editor::CommandHistory::GetInstance()
                            .ExecuteCommand(std::move(it->second));
                        g_activeCommands.erase(it);
                    }
                }
            }

            NE::ECS::Signature sig(NE::ECS::Query::GetEntitySignature(entity));
            for (const auto& [typeIdx, compType] : componentTypeRegistry) {
                if (!sig.test(compType)) continue;

                if (typeIdx == typeid(NE::ECS::Component::Transform)) {
                    auto& comp = NE::ECS::Query::GetEntityTransform(entity);
                    ImGui::SeparatorText("Transform");
                    //NE::Core::ForEachFieldView<NE::ECS::Component::Transform>(comp,
                    //    [&](auto const& desc, auto const& currentValue) {
                    //        using FieldT = std::decay_t<decltype(currentValue)>;

                    //        FieldT edited = currentValue;

                    //        if (DrawField(desc, edited)) {
                    //            SubmitSetFieldCommand(entity, desc, currentValue, edited);
                    //        }
                    //    });
                    NE::Core::ForEachFieldView<NE::ECS::Component::Transform>(comp,
                        [&](auto const& desc, auto const& currentValue) {
                            using Owner = NE::ECS::Component::Transform;
                            using FieldT = std::decay_t<decltype(currentValue)>;

                            FieldT edited = currentValue;

                            ImGui::PushID(desc.name.data());
                            const bool changed = DrawField(desc, edited);
                            const bool activated = ImGui::IsItemActivated();
                            const bool active = ImGui::IsItemActive();
                            const bool deactivated = ImGui::IsItemDeactivatedAfterEdit();
                            ImGui::PopID();

                            FieldKey key{
                                entity,
                                &typeid(Owner),
                                MemberPointerHasher<Owner, FieldT>{}(desc.member)
                            };

                            if (activated) {
                                using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
                                auto cmd = std::make_unique<Cmd>(
                                    entity,
                                    std::string("Set Transform") + desc.name.data(),
                                    desc.member,
                                    currentValue,
                                    currentValue,
                                    &NE::ECS::Command::GetEntityTransform
                                );
                                g_activeCommands[key] = std::move(cmd);
                            }

                            if (active && changed) {
                                auto it = g_activeCommands.find(key);
                                if (it != g_activeCommands.end()) {
                                    using Cmd = Editor::SetFieldCommand<Owner, FieldT>;
                                    Cmd tmp(
                                        entity,
                                        std::string{},
                                        desc.member,
                                        currentValue,
                                        edited,
                                        &NE::ECS::Command::GetEntityTransform
                                    );
                                    it->second->CoalesceFrom(tmp);
                                }
                            }

                            if (deactivated) {
                                auto it = g_activeCommands.find(key);
                                if (it != g_activeCommands.end()) {
                                    auto* asSet = dynamic_cast<Editor::SetFieldCommand<Owner, FieldT>*>(it->second.get());
                                    if (asSet && Equal(asSet->Before(), asSet->After())) {
                                        g_activeCommands.erase(it);
                                    } else {
                                        Editor::CommandHistory::GetInstance()
                                            .ExecuteCommand(std::move(it->second));
                                        g_activeCommands.erase(it);
                                    }
                                }
                            }
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

                    //NE::Core::ForEachFieldView<NE::ECS::Component::Light>(comp, [](auto&& desc, auto& field) {
                    //    DrawField(desc, field);
                    //    });
                } else if (typeIdx == typeid(NE::ECS::Component::Collider)) {
                    auto& comp = NE::ECS::Query::GetEntityCollider(entity);
                    ImGui::SeparatorText("Collider");
                    NE::Core::ForEachFieldView<NE::ECS::Component::Collider>(comp,
                        [&](auto const& desc, auto const& currentValue) {
                            using FieldT = std::decay_t<decltype(currentValue)>;

                            // make a local editable copy
                            FieldT edited = currentValue;

                            // render widget; returns true if user changed it
                            if (DrawField(desc, edited)) {
                                // don't write to comp.* here; push a command to the engine:
                                //SubmitSetFieldCommand(entity, desc, edited);
                            }
                        });
                } else if (typeIdx == typeid(NE::ECS::Component::Rigidbody)) {
                    auto& comp = NE::ECS::Query::GetEntityRigidbody(entity);
                    ImGui::SeparatorText("Rigidbody");
                    NE::Core::ForEachFieldView<NE::ECS::Component::Rigidbody>(comp,
                        [&](auto const& desc, auto const& currentValue) {
                            using FieldT = std::decay_t<decltype(currentValue)>;

                            // make a local editable copy
                            FieldT edited = currentValue;

                            // render widget; returns true if user changed it
                            if (DrawField(desc, edited)) {
                                // don't write to comp.* here; push a command to the engine:
                                //SubmitSetFieldCommand(entity, desc, edited);
                            }
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
