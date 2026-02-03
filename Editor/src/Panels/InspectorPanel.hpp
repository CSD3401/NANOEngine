#pragma once

#include "IPanel.hpp"
#include <vector>
#include <string>
#include <memory>
#include <ECS/Core/Component.hpp>
#include <Graphics/Core/Material.hpp>
#include "../AssetManagement/Interfaces/MaterialEditor.hpp"
#include "../AssetManagement/Interfaces/ModelSettingsEditor.hpp"
#include "../AssetManagement/Interfaces/TextureSettingsEditor.hpp"

namespace Editor {

	class InspectorPanel : public IPanel {
	public:
		InspectorPanel();

		void OnImGuiRender() override;
	private:
		using DrawFn = void(InspectorPanel::*)(uint32_t entity);

		struct Drawer {
			NE::ECS::ComponentType id;
			const char* name;
			DrawFn draw;
		};

		std::vector<Drawer> m_drawers;

		//void RenderTextureImportSettings(std::string metaPath);

		//void RenderModelImportSettings(const std::string& metaPath);

		void DrawEntityMetaComponent(uint32_t entity);
		void DrawPrefabInstanceComponent(uint32_t entity);
		void DrawTransformComponent(uint32_t entity);
		void DrawRendererComponent(uint32_t entity);
		void DrawRigidbodyComponent(uint32_t entity);
		void DrawColliderComponent(uint32_t entity);
		void DrawLightComponent(uint32_t entity);
		void DrawAudioSourceComponent(uint32_t entity);
		void DrawScriptComponent(uint32_t entity);
		void DrawCameraComponent(uint32_t entity);
		void DrawAnimatorComponent(uint32_t entity);
		void DrawRectTransformComponent(uint32_t entity);
		void DrawCanvasComponent(uint32_t entity);
		void DrawImageComponent(uint32_t entity);
		void DrawCharacterControllerComponent(uint32_t entity);
		

		std::unique_ptr<MaterialEditor> m_materialEditor;
		std::unique_ptr<ModelSettingsEditor> m_modelEditor;
		std::unique_ptr<TextureSettingsEditor> m_textureEditor;
		std::string m_lastPath;

	};
}
