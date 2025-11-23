#include "MaterialEditor.hpp"
#include <fstream>
#include <Engine.hpp>
#include <imgui/imgui.h>
#include <imgui/widgets/imsearch/imsearch.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include "src/EditorUI.hpp"
#include "src/AssetManagement/AssetManager.hpp"

namespace Editor {

	bool MaterialEditor::LoadMaterial(std::string filePath, std::string uuid) {
		m_path = filePath;
		m_material = NE::LoadMaterial(uuid);

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

        bool openShaderPopup = false;
        std::string uuidToName = AssetManager::GetInstance().RetrieveFileName(mat.GetPipeline()->GetSpecification().shaderName);
        DrawAssetField("Shader", uuidToName.c_str(), "+", 0.f, &openShaderPopup);
        if (openShaderPopup) ImGui::OpenPopup("AssetPicker_Shader");

        if (ImGui::BeginPopup("AssetPicker_Shader")) {
            ImGui::Text("Select a Shader");
            ImGui::Separator();
            auto& shaderList = AssetManager::GetInstance().GetAssetsOfType<AssetType::Shader>();

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
            int i = val;
            if (ImGui::DragInt(name.c_str(), &i)) mat.SetUniformInt(name, i);
        }

        ImGui::SeparatorText("Textures");
        for (auto& [uname, tex] : mat.GetTextures()) {
            DrawTextureField(uname.c_str(), tex, 48.f,
                [&](const std::string& uuid) {
                    mat.SetTexture(uname, uuid);
                    std::string has = "u_Has" + uname.substr(2);
                    if (mat.GetIntUniforms().contains(has))
                        mat.SetUniformInt(has, uuid != "" ? 1 : 0);
                });
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
            doc.AddMember("DepthTest", spec.EnableDepthTest, alloc);
            doc.AddMember("BlendMode", spec.EnableBlending, alloc);
            doc.AddMember("CullMode", spec.CullMode, alloc);
            doc.AddMember("PolygonMode", spec.PolygonMode, alloc);
        }

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

        AssetManager::GetInstance().ReimportAsset(m_path);
	}

}

