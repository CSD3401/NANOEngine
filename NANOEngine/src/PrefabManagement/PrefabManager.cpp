#include "PrefabManager.hpp"
#include <unordered_set>
#include "SceneManagement/Scene.hpp"
#include "ECS/Core/ECSCoordinator.hpp"
#include "ECS/Components/EntityMeta.hpp"
#include "ECS/Components/Transform.hpp"

namespace NE::Prefab {

	void PrefabManager::Init(SceneManagement::Scene* scene) {
        s_scene = scene;
        s_instances.clear();
        s_entityToInstance.clear();
        s_nextInstanceId = 1;
	}

	PrefabManager::InstanceInfo PrefabManager::Instantiate(const UUID& prefabAsset, std::vector<uint32_t>& newEntities) {
        InstanceInfo info{};

        if (!s_scene)
            return info;

        auto& ecs = s_scene->GetECSCoordinator();

        uint64_t id = s_nextInstanceId++;

        if (newEntities.empty())
            return info; // invalid

        info.instanceId = id;
        info.prefabAsset = prefabAsset;
        info.entities = newEntities;

        std::unordered_set<uint32_t> batch(newEntities.begin(), newEntities.end());
        uint32_t root = NE::ECS::NO_ENTITY;
        for (uint32_t e : newEntities) {
            uint32_t p = s_scene->GetECSCoordinator().GetComponent<ECS::Component::Transform>(e).parent;
            if (p == NE::ECS::NO_ENTITY || !batch.count(p)) {
                root = e;
                break;
            }
        }
        info.rootEntity = root;

        for (uint32_t e : newEntities) {
            auto& meta = ecs.GetComponent<ECS::Component::EntityMeta>(e);
            meta.prefabID = prefabAsset;
            meta.prefabInstanceID = id;
            meta.isPrefabRoot = (e == root);

            s_entityToInstance[e] = id;
        }

        s_instances[id] = info;
        return info;
	}

	void PrefabManager::DestroyInstance(uint64_t instanceId) {
        if (!s_scene)
            return;

        auto it = s_instances.find(instanceId);
        if (it == s_instances.end())
            return;

        auto& ecs = s_scene->GetECSCoordinator();

        for (uint32_t e : it->second.entities) {
            s_entityToInstance.erase(e);

            if (ecs.HasComponent<NE::ECS::Component::EntityMeta>(e)) {
                auto& meta = ecs.GetComponent<NE::ECS::Component::EntityMeta>(e);
                meta.prefabInstanceID = 0;
                meta.isPrefabRoot = false;
                meta.prefabID.clear();
            }

            ecs.DestroyEntity(e);
        }

        s_instances.erase(it);
	}

    const PrefabManager::InstanceInfo* PrefabManager::GetInstance(uint64_t instanceId) {
        auto it = s_instances.find(instanceId);
        return (it == s_instances.end()) ? nullptr : &it->second;
    }

    uint64_t PrefabManager::GetInstanceId(uint32_t entity) {
        auto it = s_entityToInstance.find(entity);
        return (it == s_entityToInstance.end()) ? 0 : it->second;
    }

    uint32_t PrefabManager::GetRootOfInstance(uint64_t instanceId) {
        auto it = s_instances.find(instanceId);
        return (it == s_instances.end()) ? NE::ECS::NO_ENTITY : it->second.rootEntity;
    }
}