#include "DecalProjectorSystem.hpp"
#include "ECS/Components/EntityMeta.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/DecalProjector.hpp"

#include "ResourceManagement/ResourceManager.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"

namespace NE::ECS::Systems {
	DecalProjectorSystem::DecalProjectorSystem(ComponentManager* cm, Core::LUIDRegistry* lr)
		: m_componentManager(cm), m_luidRegistry(lr) { }

	void DecalProjectorSystem::OnEntityAdded(Entity entity) {
		auto& decal = m_componentManager->GetComponent<Component::DecalProjector>(entity);

		if (!decal.materialUUID.empty()) {
			decal.material = Resource::ResourceManager::GetInstance().
				LoadResource<Graphics::Material>(decal.materialUUID);
		} else {
			decal.material = Resource::ResourceManager::GetInstance().
				LoadResource<Graphics::Material>("nelitmat");
		}

		if (decal.luid == 0)
			decal.luid = Core::LUIDGenerator::Generate("dp");

		m_luidRegistry->Register(decal.luid, &decal, entity);
	}

	void DecalProjectorSystem::OnEntityRemoved(Entity entity) {

	}

	void DecalProjectorSystem::Init() {

	}

	void DecalProjectorSystem::Update(double deltaTime) {

	}

	void DecalProjectorSystem::Exit() {

	}
}
