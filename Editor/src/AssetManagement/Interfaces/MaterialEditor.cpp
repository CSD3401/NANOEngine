#include "MaterialEditor.hpp"

#include <fstream>

#include <imgui/imgui.h>
#include <imgui/widgets/imsearch/imsearch.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <EditorInterface/RendererExports.hpp>
#include <Core/SpdLogger.hpp>

#include "../../EditorUI.hpp"
#include "../../AssetManagement/AssetManager.hpp"

namespace Editor {

	bool MaterialEditor::LoadMaterial(std::string filePath, std::string uuid) {
		m_path = filePath;
        m_material = NE::Renderer::Command::GetMaterial(uuid);

		return true;
	}

	void MaterialEditor::RenderSettings() {
        if (!m_material) {
            ImGui::TextDisabled("Material Failed To Load");
            return;
        }
        auto& mat = *m_material;

        ImGui::Text(m_path.c_str());
        ImGui::Separator();

        // =========================
        // Shader picker
        // =========================
        bool openShaderPopup = false;
        std::string uuidToName = Assets::AssetManager::GetInstance().RetrieveFilename(mat.GetPipeline()->GetSpecification().shaderName);
        DrawAssetField("Shader", uuidToName, &openShaderPopup);
        if (openShaderPopup) ImGui::OpenPopup("AssetPicker_Shader");

        if (ImGui::BeginPopup("AssetPicker_Shader")) {
            ImGui::Text("Select a Shader");
            ImGui::Separator();
            auto& shaderList = Assets::AssetManager::GetInstance().GetAssetsOfType(Assets::AssetType::Shader);

            if (ImSearch::BeginSearch()) {
                ImSearch::SearchBar();
                for (const auto& [shaderName, uuid] : shaderList) {
                    ImSearch::SearchableItem(shaderName.c_str(), [&, shaderName](const char*) {
                        if (ImGui::Selectable(shaderName.c_str())) {
                            m_material->SetShader(uuid);
                            ImGui::CloseCurrentPopup();
                        }
                        });
                }
                ImSearch::EndSearch();
            }
            ImGui::EndPopup();
        }

        // =========================
        // Uniforms
        // =========================
        ImGui::SeparatorText("Uniforms");

        for (auto& [name, val] : mat.GetFloatUniforms()) {
            float f = val;
            if (Editor::DrawFloatControl(name.c_str(), f, 0.1f)) mat.SetUniformFloat(name, f);
        }

        for (auto& [name, val] : mat.GetVec3Uniforms()) {
            auto v = val;
            if (Editor::DrawVec3Control(name.c_str(), v, 0.f, 100.f)) mat.SetUniformVec3(name, v);
        }

        for (auto& [name, val] : mat.GetIntUniforms()) {
            if (name.rfind("h_", 0) == 0) continue;
            int i = val;
            if (ImGui::DragInt(name.c_str(), &i)) mat.SetUniformInt(name, i);
        }

        ImGui::SeparatorText("Textures");
        for (auto& [uname, tex] : mat.GetTextures()) {
            DrawTextureField(uname.c_str(), tex, 48.f,
                [&](const std::string& uuid) {
                    mat.SetTexture(uname, uuid);
                    std::string has = "h_Has" + uname.substr(2);
                    if (mat.GetIntUniforms().contains(has))
                        mat.SetUniformInt(has, uuid != "" ? 1 : 0);
                });
        }

        // =========================
        // Pipeline Settings
        // =========================
        if (ImGui::CollapsingHeader("Pipeline Settings")) {
            // Copy the pipeline spec (do not edit shared pipeline state)
            NE::Graphics::PipelineSpecification spec = mat.GetPipeline()->GetSpecification();

            bool pipelineChanged = false;

            pipelineChanged |= ImGui::Checkbox("Blending", &spec.EnableBlending);
            pipelineChanged |= ImGui::Checkbox("Depth Test", &spec.EnableDepthTest);
			pipelineChanged |= ImGui::Checkbox("Depth Write", &spec.DepthWrite);

            // Note: CullMode and PolygonMode are currently serialized directly as GL enums, 
            // but should be abstracted later because exposing GL enums directly is not ideal for cross-API compatibility

            // ---- Cull Mode ----
            const char* cullItems[] = { "None", "Back", "Front" };

            constexpr int CULL_NONE = 0;
            constexpr int CULL_BACK = 0x0405; // GL_BACK
            constexpr int CULL_FRONT = 0x0404; // GL_FRONT

            auto CullEnumToIndex = [](int v) -> int {
                switch (v) {
                case CULL_NONE:           return 0;
                case CULL_BACK:           return 1;
                case CULL_FRONT:          return 2;
                default:                  return 1; // default Back
                }
                };
            auto CullIndexToEnum = [](int i) -> int {
                switch (i) {
                case 0:  return CULL_NONE;
                case 1:  return CULL_BACK;
                case 2:  return CULL_FRONT;
                default: return CULL_BACK;
                }
                };

            int cullIdx = CullEnumToIndex(spec.CullMode);
            if (ImGui::Combo("Cull Mode", &cullIdx, cullItems, IM_ARRAYSIZE(cullItems))) {
                spec.CullMode = CullIndexToEnum(cullIdx);
                pipelineChanged = true;
            }

            // ---- Polygon Mode ----
            const char* polyItems[] = { "Fill", "Wireframe", "Point" };

            constexpr int POLY_FILL = 0x1B02; // GL_FILL
            constexpr int POLY_LINE = 0x1B01; // GL_LINE
            constexpr int POLY_POINT = 0x1B00; // GL_POINT

            auto PolyEnumToIndex = [](int v) -> int {
                switch (v) {
                case POLY_FILL:  return 0;
                case POLY_LINE:  return 1;
                case POLY_POINT: return 2;
                default:         return 0; // default Fill
                }
                };
            auto PolyIndexToEnum = [](int i) -> int {
                switch (i) {
                case 0:  return POLY_FILL;
                case 1:  return POLY_LINE;
                case 2:  return POLY_POINT;
                default: return POLY_FILL;
                }
                };

            int polyIdx = PolyEnumToIndex(spec.PolygonMode);
            if (ImGui::Combo("Polygon Mode", &polyIdx, polyItems, IM_ARRAYSIZE(polyItems))) {
                spec.PolygonMode = PolyIndexToEnum(polyIdx);
                pipelineChanged = true;
            }

            // Apply pipeline changes (creates/gets a NEW cached pipeline; does NOT mutate existing)
            if (pipelineChanged) {
                mat.ApplyPipelineSpec(spec);
            }

            ImGui::Separator();

            // =========================
            // Render Queue
            // =========================
            int rqIndex = 1; // default Geometry
            switch (mat.GetQueueBase()) {
            case NE::Graphics::RenderQueue::BACKGROUND:   rqIndex = 0; break;
            case NE::Graphics::RenderQueue::GEOMETRY:     rqIndex = 1; break;
            case NE::Graphics::RenderQueue::ALPHATEST:    rqIndex = 2; break;
            case NE::Graphics::RenderQueue::TRANSPARENT:  rqIndex = 3; break;
            case NE::Graphics::RenderQueue::OVERLAY:      rqIndex = 4; break;
            default: rqIndex = 1; break;
            }

            const char* rqItems[] = { "Background", "Geometry", "AlphaTest", "Transparent", "Overlay" };
            if (ImGui::Combo("Render Queue", &rqIndex, rqItems, IM_ARRAYSIZE(rqItems))) {
                using RQ = NE::Graphics::RenderQueue;
                switch (rqIndex) {
                case 0: mat.SetQueueBase(RQ::BACKGROUND); break;
                case 1: mat.SetQueueBase(RQ::GEOMETRY); break;
                case 2: mat.SetQueueBase(RQ::ALPHATEST); break;
                case 3: mat.SetQueueBase(RQ::TRANSPARENT); break;
                case 4: mat.SetQueueBase(RQ::OVERLAY); break;
                default: mat.SetQueueBase(RQ::GEOMETRY); break;
                }
            }

            int rqOff = mat.GetQueueOffset();
            if (ImGui::DragInt("Queue Offset", &rqOff, 1.0f, -500, 500)) {
                mat.SetQueueOffset(rqOff);
            }

            ImGui::Text("Final Queue: %u", mat.GetQueueOrder());
        }

        if (ImGui::Button("Save Material", { 120, 28 })) {
            Save();
        }
	}

	void MaterialEditor::Save() {
        rapidjson::Document doc;
        doc.SetObject();
        rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();

        auto& mat = *m_material;

        if (mat.m_Pipeline) {
            doc.AddMember("Shader", rapidjson::Value(mat.m_Pipeline->GetSpecification().shaderName.data(), alloc).Move(), alloc);
            auto spec = mat.m_Pipeline->GetSpecification();
            doc.AddMember("BlendMode", spec.EnableBlending, alloc);
            doc.AddMember("DepthTest", spec.EnableDepthTest, alloc);
			doc.AddMember("DepthWrite", spec.DepthWrite, alloc);
            doc.AddMember("CullMode", spec.CullMode, alloc);
            doc.AddMember("PolygonMode", spec.PolygonMode, alloc);
        }
        switch (mat.GetQueueBase()) {
        case NE::Graphics::RenderQueue::BACKGROUND:
            doc.AddMember("RenderQueueBase", "Background", alloc);
            break;
        case NE::Graphics::RenderQueue::GEOMETRY:
            doc.AddMember("RenderQueueBase", "Geometry", alloc);
            break;
        case NE::Graphics::RenderQueue::ALPHATEST:
            doc.AddMember("RenderQueueBase", "AlphaTest", alloc);
            break;
        case NE::Graphics::RenderQueue::TRANSPARENT:
            doc.AddMember("RenderQueueBase", "Transparent", alloc);
            break;
        case NE::Graphics::RenderQueue::OVERLAY:
            doc.AddMember("RenderQueueBase", "Overlay", alloc);
            break;
        default:
            doc.AddMember("RenderQueueBase", "Geometry", alloc);
            SPD_WARNING("Unknown RenderQueueBase detected during material save, defaulting to GEOMETRY");
            break;
        }
        doc.AddMember("RenderQueueOffset", mat.GetQueueOffset(), alloc);

        rapidjson::Value uniforms(rapidjson::kObjectType);

        for (const auto& [name, value] : mat.m_IntUniforms) {
            uniforms.AddMember(rapidjson::Value(name.c_str(), alloc).Move(), rapidjson::Value(value).Move(), alloc);
        }
        for (const auto& [name, value] : mat.m_FloatUniforms) {
            uniforms.AddMember(rapidjson::Value(name.c_str(), alloc).Move(), rapidjson::Value(value).Move(), alloc);
        }
        for (const auto& [name, value] : mat.m_Vec3Uniforms) {
            rapidjson::Value arr(rapidjson::kArrayType);
            arr.PushBack(value.x, alloc).PushBack(value.y, alloc).PushBack(value.z, alloc);
            uniforms.AddMember(rapidjson::Value(name.c_str(), alloc).Move(), arr, alloc);
        }
        // Mat4 uniforms (if needed)
        // for (const auto& [name, value] : m_Mat4Uniforms) {
        //     Value arr(kArrayType);
        //     for (int i = 0; i < 16; ++i) arr.PushBack(value.data[i], alloc); // adjust based on your Mat4 storage
        //     uniforms.AddMember(Value(name.c_str(), alloc).Move(), arr, alloc);
        // }
        for (const auto& [name, tex] : mat.m_Textures) {
            std::string _uuid = tex ? tex->uuid : "";
            uniforms.AddMember(rapidjson::Value(name.c_str(), alloc).Move(), rapidjson::Value(_uuid.c_str(), alloc).Move(), alloc);
        }

        doc.AddMember("Properties", uniforms, alloc);

        // Write to file
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        std::ofstream out(m_path);
        if (out.is_open()) {
            out << buffer.GetString();
            out.flush();
        }

        Assets::AssetManager::GetInstance().ReimportAsset(m_path);
	}

}

