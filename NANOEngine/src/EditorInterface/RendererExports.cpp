#include "RendererExports.hpp"
#include "../AssetManager.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../ECS/Components/UIImage.hpp"
#include <iostream>

namespace NE {
	SceneManagement::Scene& GetScene();
}

namespace NE::Renderer {

	namespace Query {

	}

	namespace Command {
		void AssignModel(uint32_t e, std::string_view path) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			r.modelPath = std::string(path);
			r.model = Asset::AssetManager::GetInstance().Get<Graphics::Model>(path.data());

			if (r.materialPath.empty()) {
				r.materialPath = "Assets/Basic.nanomat";
				r.material = Asset::AssetManager::GetInstance().Load<Graphics::Material>("Assets/Basic.nanomat", false);
			}
		}

		void AssignMaterial(uint32_t e, std::string_view path) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			r.materialPath = std::string(path);
			r.material = Asset::AssetManager::GetInstance().Load<Graphics::Material>(path.data(), false);
		}
	}
}
