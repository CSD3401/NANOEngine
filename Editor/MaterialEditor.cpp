#include "MaterialEditor.hpp"
#include <Engine.hpp>
#include <imgui/imgui.h>
#include "src/EditorUI.hpp"
#include "src/AssetManagement/AssetManager.hpp"
#include <imgui/widgets/imsearch/imsearch.h>

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

        bool openShaderPopup = false;
        DrawAssetField("Shader", mat.GetPipeline()->GetSpecification().shaderName, "+", 0.f, &openShaderPopup);
        if (openShaderPopup) ImGui::OpenPopup("PickShader");

        if (ImGui::BeginPopup("PickShader")) {
            ImGui::Text("Select Shader");
            ImGui::Separator();
            if (ImSearch::BeginSearch()) {
                ImSearch::SearchBar();
                for (const auto& [shaderName, uuid] : AssetManager::GetInstance().GetInstance().GetAssetsOfType<AssetType::Shader>()) {
                    ImSearch::SearchableItem(shaderName.c_str(), [&, shaderName](const char*) {
                        if (ImGui::Selectable(shaderName.c_str())) {
                            //mat.SetShader(shaderName);
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

        //ImGui::SeparatorText("Textures");
        //for (auto& [uname, tex] : mat.GetTextures()) {
        //    DrawTextureField(uname.c_str(), tex, 96.f,
        //        [&](const std::string& uuid) {
        //            auto newTex = Asset::AssetManager::GetInstance().Load<OpenGL::GLTexture>(uuid);
        //            mat.SetTexture(uname, newTex);
        //            std::string has = "u_Has" + uname.substr(2);
        //            if (mat.GetIntUniforms().contains(has))
        //                mat.SetUniformInt(has, newTex ? 1 : 0);
        //        });
        //}

        if (ImGui::Button("Save Material", { 120, 28 })) {
            Save();
        }
	}

	void MaterialEditor::Save() {

	}

}

