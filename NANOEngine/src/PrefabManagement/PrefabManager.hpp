#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include "ECS/Core/Entity.hpp"

namespace NE::SceneManagement {
    class SceneManager;
}

namespace NE::Prefab {

    class PrefabManager {
        using UUID = std::string;
    public:
        //struct InstanceInfo {
        //    uint64_t              instanceId = 0;
        //    UUID                  prefabAsset;
        //    uint32_t              rootEntity = NE::ECS::NO_ENTITY;
        //    std::vector<uint32_t> entities;
        //};

        static void Init(SceneManagement::SceneManager* scene);
        //static InstanceInfo Instantiate(const UUID& prefabAsset, 
        //    std::vector<uint32_t>& newEntities);

        static void Instantiate(const UUID& prefabAsset, uint32_t rootEntity);

        static void DestroyInstance(const UUID& prefabAsset, uint32_t instanceId);
        //static const InstanceInfo* GetInstance(uint64_t instanceId);

        static void ReloadAllInstancesOfPrefab(const UUID& prefabAsset);
        static void RebuildFromScene();

        static uint64_t GetInstanceId(uint32_t entity);
        static uint32_t GetRootOfInstance(uint64_t instanceId);

    private:
        inline static SceneManagement::SceneManager* s_sceneManager = nullptr;
        inline static uint64_t s_nextInstanceId = 1;
        //inline static std::unordered_map<uint64_t, InstanceInfo> s_instances;
        //inline static std::unordered_map<uint32_t, uint64_t> s_entityToInstance;

		inline static std::unordered_map<UUID, std::vector<uint32_t>> s_prefabToInstances;
    };


}

