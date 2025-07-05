#include "InspectorPanel.hpp"
#include <imgui/imgui.h>
#include <ECSInternals.hpp>
#include "src/ECS/Core/Signature.hpp"
#include <src/ECS/Components/Transform.hpp>
#include <src/ECS/Components/Renderer.hpp>
#include <src/Core/Reflection.hpp>
#include <src/Math/Vec3.hpp>
#include "../EditorScene.hpp"

namespace {
    template<typename Owner, typename T>
    void DrawField(const NANOEngine::Core::FieldDescriptor<Owner, T>& desc, T& value) {
        using namespace NANOEngine;
        if constexpr (std::is_same_v<T, bool>) {
            ImGui::Checkbox(desc.name.data(), &value);
        } else if constexpr (std::is_same_v<T, int>) {
            ImGui::DragInt(desc.name.data(), &value);
        } else if constexpr (std::is_same_v<T, float>) {
            ImGui::DragFloat(desc.name.data(), &value, 0.1f);
        } else if constexpr (std::is_same_v<T, Math::Vec3>) {
            ImGui::DragFloat3(desc.name.data(), value.Data(), 0.1f);
        } else {
            ImGui::Text("%s (unsupported)", desc.name.data());
        }
    }
}

namespace Editor {
	InspectorPanel::InspectorPanel() {

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
                    Core::ForEachField<ECS::Component::Transform>(comp, [](auto&& desc, auto& field) {
                        DrawField(desc, field);
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
                            //comp.modelPath = dropped;
                            //comp.model = Graphics::LoadModel(dropped);
                        }
                        ImGui::EndDragDropTarget();
                    }
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

        }
        ImGui::End();
    }
}
