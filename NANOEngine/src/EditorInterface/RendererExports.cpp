#include "RendererExports.hpp"
#include "../AssetManager.hpp"
#include "../ECS/Components/Renderer.hpp"

namespace NE::Renderer {

	namespace Query {

	}

	namespace Command {
		NANOENGINE_API void AssignRendererModel(NE::ECS::Component::Renderer& r, std::string filepath) {
			r.modelPath = filepath;
			r.model = Asset::AssetManager::GetInstance().Get<Graphics::Model>(filepath);
		}

		NANOENGINE_API void AssignRendererMaterial(NE::ECS::Component::Renderer& r, std::string filepath) {
			r.materialPath = filepath;
			r.material = Asset::AssetManager::GetInstance().Load<Graphics::Material>(filepath, false);
		}
	}

}
