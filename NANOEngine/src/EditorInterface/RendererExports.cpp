#include "pch.h"
#include <glad/glad.h>
#include "RendererExports.hpp"
#include "SceneManagement/Scene.hpp"

#include "ResourceManagement/ResourceManager.hpp"
#include "../Graphics/Core/PipelineCache.hpp"
#include "Core/SpdLogger.hpp"
#include "../../include/ScriptSDK/ScriptTypes.h"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/Core/RenderGraph.hpp"

#include "ECS/Components/Renderer.hpp"
#include "ECS/Components/DecalProjector.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/UIImage.hpp"
#include "ECS/Components/UICanvas.hpp"
#include "ECS/Components/UIRectTransform.hpp"
#include "ECS/Components/PERenderer.hpp"

namespace NE {
	SceneManagement::Scene& GetScene();

	namespace Scripting {
		// Forward declaration of helper function from ScriptAPI.cpp
		std::string GetMaterialUUIDFromRef(const MaterialRef& materialRef);
	}
}

namespace NE::Renderer {

	namespace {
		void ConfigureDecalMaterial(const std::shared_ptr<Graphics::Material>& material) {
			if (!material || !material->GetPipeline()) return;

			material->SetShader("nedecalprojected");
			material->SetQueueBase(Graphics::RenderQueue::OVERLAY);
			material->SetQueueOffset(0);

			if (material->GetPipeline()) {
				auto spec = material->GetPipeline()->GetSpecification();
				spec.EnableBlending = true;
				spec.EnableDepthTest = false;
				spec.DepthWrite = false;
				spec.CullMode = GL_NONE;
				spec.PolygonMode = GL_FILL;
				material->ApplyPipelineSpec(spec);
			}
		}
	}

	namespace Query {
		std::string GetModel(uint32_t e) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			if (r.modelUUID.empty()) return "empty uuid";
			else return r.modelUUID;
		}

		std::string GetMaterial(uint32_t e) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			if (r.materialUUID.empty()) return "empty uuid";
			else return r.materialUUID;
		}

		const Graphics::RenderSettings& GetRenderSettings() {
			return Graphics::GraphicsManager::renderSettings;
		}

		const Graphics::PostProcessingSettings& GetPostProcessingSettings() {
			return Graphics::GraphicsManager::postProcessingSettings;
		}

		Graphics::RenderGraph* GetRenderGraph() {
			return Graphics::GraphicsManager::GetRenderGraph();
		}

		Graphics::TexturePool* GetTexturePool() {
			return Graphics::GraphicsManager::GetTexturePool();
		}

		NANOENGINE_API std::string GetMaterialUUID(const NE::Scripting::MaterialRef& materialRef)
		{
			return NE::Scripting::GetMaterialUUIDFromRef(materialRef);
		}
	}

	namespace Command {
		std::shared_ptr<NE::Graphics::Material> GetMaterial(const std::string& uuid) {
			return Resource::ResourceManager::GetInstance().LoadResource<NE::Graphics::Material>(uuid);
		}

		void AssignModel(uint32_t e, const std::string& uuid) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			r.modelUUID = uuid;
			r.model = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Model>(uuid);

			if (r.materialUUID.empty()) {
				r.materialUUID = "neunlitmat";
				r.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>("neunlitmat");
			}
		}

		void AssignModel(uint32_t e, const std::string& uuid, int32_t submeshIndex) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			r.modelUUID = uuid;
			r.model = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Model>(uuid);
			r.subMeshIndex = submeshIndex;

			if (r.materialUUID.empty()) {
				r.materialUUID = "neunlitmat";
				r.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>("neunlitmat");
			}
		}

		void AssignMaterial(uint32_t e, const std::string& uuid) {
			if (GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::Renderer>(e)) {
				auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
				r.materialUUID = uuid;
				r.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>(uuid);
			} 
			else if (GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::DecalProjector>(e)) {
				auto& d = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::DecalProjector>(e);
				d.materialUUID = uuid;
				std::shared_ptr<Graphics::Material> sourceMaterial; 
				sourceMaterial = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>(uuid);

				if (sourceMaterial) {
					d.material = std::make_shared<Graphics::Material>(*sourceMaterial);
					ConfigureDecalMaterial(d.material);
				}
			}
			else if (GetScene().GetECSCoordinator().HasComponent<NE::ECS::Component::PERenderer>(e)) {
				auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::PERenderer>(e);
				r.materialUUID = uuid;
				r.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>(uuid);
			}
		}

		Graphics::RenderSettings& GetRenderSettings() { return Graphics::GraphicsManager::renderSettings; }
		Graphics::PostProcessingSettings& GetPostProcessingSettings() { return Graphics::GraphicsManager::postProcessingSettings; }

		void DrawSelectedLightGizmos(const NE::ECS::Component::Light& lightComponent) {
			Graphics::GraphicsManager::DrawSelectedLightGizmos(lightComponent);
		}

		void DrawSelectedDecalGizmos(const NE::ECS::Component::DecalProjector& decalComponent,
			const NE::ECS::Component::Transform& transformComponent
		) {
			Graphics::GraphicsManager::DrawSelectedDecalGizmos(decalComponent, transformComponent);
		}

		void SetSelectedEntities(const std::vector<uint32_t>& selectedIds) {
			Graphics::GraphicsManager::SetSelectedEntities(selectedIds);
		}

		void ClearSelectedEntities() {
			Graphics::GraphicsManager::ClearSelectedEntities();
		}

        void AssignUITexture(uint32_t e, const std::string& textureUUID, const std::string& materialUUID) {
            auto& img = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIImage>(e);

            // load the material (this loads the shader, pipeline settings, etc.)
			if (materialUUID.empty())
			{
				SPD_ERROR("[AssignUITexture] Material UUID is empty!");
				return;
			}
			
			// store uuids for serialization
            img.textureUUID = textureUUID;
			img.materialUUID = materialUUID;

			img.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>(materialUUID);

			if (!img.material)
			{
				SPD_ERROR("[AssignUITexture] Failed to load material with UUID: " << materialUUID);
				return;
			}

			// load texture and get bindless handle
			if (!textureUUID.empty()) {
				auto texture = Resource::ResourceManager::GetInstance().LoadResource<Graphics::OpenGL::GLTexture>(textureUUID);
				if (texture) {
					texture->MakeResident(); // CRITICAL: Make texture resident for bindless access
					img.bindlessHandle = texture->GetBindlessHandle();
				}
			}

			// Mark as dirty so renderer picks up the change
			img.isDirty = true;
        }
	}
}
