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
			r.modelPath = uuid;
			r.model = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Model>(uuid);
			//r.model = Asset::AssetManager::GetInstance().Get<Graphics::Model>(path.data());

			if (r.materialPath.empty()) {
				r.materialPath = "neunlitmat";
				r.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>("neunlitmat");
				//r.material = Asset::AssetManager::GetInstance().Load<Graphics::Material>("Assets/Basic.nanomat", false);
			}
		}

		void AssignMaterial(uint32_t e, const std::string& uuid) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			r.materialPath = uuid;
			r.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>(uuid);
			//r.material = Asset::AssetManager::GetInstance().Load<Graphics::Material>(path.data(), false);
		}
	}

}
