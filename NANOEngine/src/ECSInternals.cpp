#include "ECSInternals.hpp"
#include "SceneManagement/Scene.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/Renderer.hpp"
#include "ECS/Components/Light.hpp"
#include "AssetManager.hpp"

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

	ECS::Component::Renderer& GetEntityRenderer(uint32_t e) {
		return GetScene().GetECSCoordinator().GetComponent<ECS::Component::Renderer>(e);
	}

	ECS::Component::Light& GetEntityLight(uint32_t e)
	{
		return GetScene().GetECSCoordinator().GetComponent<ECS::Component::Light>(e);
	}

	void AddLightComponent(uint32_t e)
	{
		GetScene().GetECSCoordinator().AddComponent(e, ECS::Component::Light{});
	}

	void AssignRendererModel(ECS::Component::Renderer& r, std::string filepath) {
		r.modelPath = filepath;
		r.model = Graphics::LoadModel(filepath);
	}

	void AssignRendererMaterial(ECS::Component::Renderer& r, std::string filepath) {
		r.materialPath = filepath;
		r.material = Asset::AssetManager::GetInstance().Load<Graphics::Material>(filepath, false);
	}

	const std::unordered_map<std::type_index, uint8_t>& GetRegisteredComponentTypes()
	{
		return GetScene().GetECSCoordinator().GetRegisteredComponentTypes();
	}

	std::vector<uint32_t>& GetEntities()
	{
		return GetScene().GetECSCoordinator().GetUsedEntities();
	}


}