#include "RendererExports.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "../EngineState.hpp"  // For GetEngineState
#include "../Engine.hpp"  // For MarkSceneDirty
#include <Core/SpdLogger.hpp>  

namespace NE {
	SceneManagement::Scene& GetScene();
}

namespace NE::Renderer {

	namespace Query {
		NANOENGINE_API std::string GetModel(uint32_t e)
		{
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			if (r.modelUUID.empty()) return "empty uuid";
			else return r.modelUUID;
		}
		NANOENGINE_API std::string GetMaterial(uint32_t e)
		{
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			if (r.materialUUID.empty()) return "empty uuid";
			else return r.materialUUID;
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
	}

}
