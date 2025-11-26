#include "RendererExports.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../ECS/Components/UIImage.hpp"
#include "../ECS/Components/UICanvas.hpp"
#include "../ECS/Components/UIRectTransform.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "../../Editor/src/AssetManagement/AssetManager.hpp"
#include "../Graphics/Core/PipelineCache.hpp"
#include "../EngineState.hpp"  // For GetEngineState
#include "../Engine.hpp"  // For MarkSceneDirty
#include <Core/SpdLogger.hpp>
#include "../../include/ScriptSDK/ScriptTypes.h"
#include <Graphics/Core/GraphicsManager.hpp>

namespace NE {
	SceneManagement::Scene& GetScene();

	namespace Scripting {
		// Forward declaration of helper function from ScriptAPI.cpp
		std::string GetMaterialUUIDFromRef(const MaterialRef& materialRef);
	}
}

namespace NE::Renderer {

	namespace Query {
		std::string GetModel(uint32_t e)
		{
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			if (r.modelUUID.empty()) return "empty uuid";
			else return r.modelUUID;
		}
		std::string GetMaterial(uint32_t e)
		{
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			if (r.materialUUID.empty()) return "empty uuid";
			else return r.materialUUID;
		}

		NANOENGINE_API std::string GetMaterialUUID(const NE::Scripting::MaterialRef& materialRef)
		{
			return NE::Scripting::GetMaterialUUIDFromRef(materialRef);
		}
	}

	namespace Command {
		void AssignModel(uint32_t e, const std::string& uuid) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			r.modelUUID = uuid;
			r.model = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Model>(uuid);

			if (r.materialUUID.empty()) {
				r.materialUUID = "neunlitmat";
				r.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>("neunlitmat");
			}
			
			// Mark component and scene dirty (Edit mode only)
			if (NE::GetEngineState() == NE::EngineState::Edit) {
				if constexpr (requires { r.isDirty; }) r.isDirty = true;
				NE::MarkSceneDirty();
				SPD_DEBUG("[DirtyFlag] Model changed - Scene marked DIRTY");
			}
		}

		void AssignMaterial(uint32_t e, const std::string& uuid) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			r.materialUUID = uuid;
			r.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>(uuid);
			
			// Mark component and scene dirty (Edit mode only)
			if (NE::GetEngineState() == NE::EngineState::Edit) {
				if constexpr (requires { r.isDirty; }) r.isDirty = true;
				NE::MarkSceneDirty();
				SPD_DEBUG("[DirtyFlag] Material changed - Scene marked DIRTY");
			}
		}

		Graphics::RenderSettings& GetRenderSettings() { return Graphics::GraphicsManager::renderSettings; }

        void AssignUITexture(uint32_t e, const std::string& textureUUID, const std::string& materialUUID) {
            auto& img = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::UIImage>(e);

            // load the material (this loads the shader, pipeline settings, etc.)
			if (materialUUID.empty())
			{
				SPD_ERROR("[AssignUITexture] Material UUID is empty!");
				return;
			}

            img.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>(materialUUID);

            if (!img.material) 
            {
                SPD_ERROR("[AssignUITexture] Failed to load material with UUID: " << materialUUID);
                return;
            }

            //// assign the texture to the material
            //img.material->SetTexture("u_BaseMap", textureUUID);

            //// Update the u_HasBaseMap flag to indicate texture is present
            //img.material->SetUniformInt("u_HasBaseMap", 1);
			
			// Load texture and get its BINDLESS HANDLE (per-instance)
			auto texture = Resource::ResourceManager::GetInstance().LoadResource<Graphics::OpenGL::GLTexture>(textureUUID);
			if (texture) {
				img.bindlessHandle = texture->GetBindlessHandle();  // Store per UI image
			}

            // Store texture UUID in component for serialization
            img.textureUUID = textureUUID;

            // mark dirty
            if (NE::GetEngineState() == NE::EngineState::Edit)
            {
                img.isDirty = true;
                NE::MarkSceneDirty();
                SPD_DEBUG("[DirtyFlag] UI Texture changed - Scene marked DIRTY");
            }
        }
	}
}
