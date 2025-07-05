#include "ECSInternals.hpp"
#include "src/SceneManagement/Scene.hpp"
#include "src/ECS/Components/Transform.hpp"
#include "src/ECS/Components/Renderer.hpp"

namespace NANOEngine {
	SceneManagement::Scene& GetScene();
	//extern SceneManagement::Scene scene;

	uint32_t CreateEntity()
	{
		return GetScene().GetECSCoordinator().CreateEntity();
	}

	void DestroyEntity(uint32_t e)
	{
		GetScene().GetECSCoordinator().DestroyEntity(e);
	}

	uint64_t GetEntitySignature(uint32_t e)
	{
		return GetScene().GetECSCoordinator().GetSignature(e).to_ullong();
	}
	
	ECS::Component::Transform& GetEntityTransform(uint32_t e) {
		return GetScene().GetECSCoordinator().GetComponent<ECS::Component::Transform>(e);
	}

	NANOENGINE_API ECS::Component::Renderer& GetEntityRenderer(uint32_t e) {
		return GetScene().GetECSCoordinator().GetComponent<ECS::Component::Renderer>(e);
	}

	void AssignRendererModel(ECS::Component::Renderer& r, std::string filepath) {
		r.modelPath = filepath;
		r.model = Graphics::LoadModel(filepath);
	}

	const std::unordered_map<std::type_index, uint8_t>& GetRegisteredComponentTypes()
	{
		return GetScene().GetECSCoordinator().GetRegisteredComponentTypes();
	}


}