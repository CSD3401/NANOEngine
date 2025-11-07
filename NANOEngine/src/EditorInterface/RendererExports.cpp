#include "RendererExports.hpp"
//#include "../AssetManager.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "ResourceManagement/ResourceManager.hpp"

namespace NE {
	SceneManagement::Scene& GetScene();
}

namespace NE::Renderer {

	namespace Query {

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
		}

		void AssignMaterial(uint32_t e, const std::string& uuid) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			r.materialUUID = uuid;
			r.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>(uuid);
		}
	}

}
