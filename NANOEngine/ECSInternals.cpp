#include "ECSInternals.hpp"
#include "src/SceneManagement/Scene.hpp"

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

}