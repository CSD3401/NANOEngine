#include "pch.h"
#include "PrefabManager.hpp"
#include <unordered_set>
#include <fstream>
#include "SceneManagement/SceneManager.hpp"
#include "ECS/Core/ECSCoordinator.hpp"
//#include "ECS/Components/EntityMeta.hpp"
//#include "ECS/Components/Transform.hpp"
#include "ECS/Components/Hierarchy.hpp"
#include "Serialisation/Serializer.hpp"
#include "ResourceManagement/ResourcePaths.hpp"

namespace NE::Prefab {

    namespace {
        void DestroyChildrenOfInstance(SceneManagement::Scene* scene, uint32_t entity, bool isRoot = false) {
            auto& ecs = scene->GetECSCoordinator();

			auto& hierarchy = ecs.GetComponent<NE::ECS::Component::Hierarchy>(entity);
            for (auto child : hierarchy.children) {
                DestroyChildrenOfInstance(scene, child);
			}
            if (!isRoot) {
				ecs.DestroyEntity(entity);
            } else {
				hierarchy.children.clear();
            }
		}
    }

	void PrefabManager::Init(SceneManagement::SceneManager* scene) {
        s_sceneManager = scene;
        //s_instances.clear();
        //s_entityToInstance.clear();
        s_prefabToInstances.clear();
        s_nextInstanceId = 1;
	}

	//PrefabManager::InstanceInfo PrefabManager::Instantiate(const UUID& prefabAsset, std::vector<uint32_t>& newEntities) {
 //       InstanceInfo info{};

 //       if (!s_scene)
 //           return info;

 //       auto& ecs = s_scene->GetECSCoordinator();

 //       uint64_t id = s_nextInstanceId++;

 //       if (newEntities.empty())
 //           return info; // invalid

 //       info.instanceId = id;
 //       info.prefabAsset = prefabAsset;
 //       info.entities = newEntities;

 //       std::unordered_set<uint32_t> batch(newEntities.begin(), newEntities.end());
 //       uint32_t root = NE::ECS::NO_ENTITY;
 //       //for (uint32_t e : newEntities) {
 //       //    uint32_t p = s_scene->GetECSCoordinator().GetComponent<ECS::Component::Transform>(e).parent;
 //       //    if (p == NE::ECS::NO_ENTITY || !batch.count(p)) {
 //       //        root = e;
 //       //        break;
 //       //    }
 //       //}
 //       info.rootEntity = root;

 //       //for (uint32_t e : newEntities) {
 //       //    auto& meta = ecs.GetComponent<ECS::Component::EntityMeta>(e);
 //       //    meta.prefabID = prefabAsset;
 //       //    meta.prefabInstanceID = id;
 //       //    meta.isPrefabRoot = (e == root);

 //       //    s_entityToInstance[e] = id;
 //       //}

 //       s_instances[id] = info;
 //       return info;
	//}

    void PrefabManager::Instantiate(const UUID& prefabAsset, uint32_t rootEntity) {
        if (s_sceneManager->IsEditingPrefab() || s_sceneManager->IsPlaying())
            return;

		s_prefabToInstances[prefabAsset].push_back(rootEntity);
    }

	void PrefabManager::DestroyInstance(const UUID& prefabAsset, uint32_t instanceId) {
        if (s_sceneManager->IsEditingPrefab() || s_sceneManager->IsPlaying())
            return;

		auto& prefabInstances = s_prefabToInstances[prefabAsset];

		prefabInstances.erase(std::remove(prefabInstances.begin(), prefabInstances.end(), instanceId), prefabInstances.end());
	}

    //const PrefabManager::InstanceInfo* PrefabManager::GetInstance(uint64_t instanceId) {
    //    auto it = s_instances.find(instanceId);
    //    return (it == s_instances.end()) ? nullptr : &it->second;
    //}

    void PrefabManager::ReloadAllInstancesOfPrefab(const UUID& prefabAsset) {
        auto editorScene = s_sceneManager->GetEditorScene();

        if (!editorScene) return;
        auto& ecs = editorScene->GetECSCoordinator();

		auto& prefabInstances = s_prefabToInstances[prefabAsset];
		std::string prefabPath = Resource::ComputeArtifactPathFromUUID(prefabAsset, NE::Resource::ResourceType::Prefab);

        for (auto instance : prefabInstances) {
			DestroyChildrenOfInstance(editorScene, instance, true);
		    Deserialization::DeserializePrefab(ecs, prefabPath, instance);
        }


        //std::ifstream in(prefabPath, std::ios::binary);
        //if (!in) return;

        //std::string data((std::istreambuf_iterator<char>(in)), {});
        //rapidjson::Document doc; 
        //doc.Parse(data.c_str());
        //if (!doc.IsObject() || !doc.HasMember("Entities")) return;

        //auto entitiesJson = doc["Entities"].GetArray();

        //std::unordered_map<uint64_t, const rapidjson::Value*> localToJson;
        //for (auto& entVal : entitiesJson) {
        //    const auto& tJson = entVal["Transform"];
        //    uint64_t lid = tJson["luid"].GetUint64();
        //    localToJson[lid] = &entVal;
        //}

        //using NE::ECS::Component::Transform;

        //for (auto& [instId, inst] : s_instances) {
        //    if (inst.prefabAsset != prefabAsset)
        //        continue;

        //    for (uint32_t e : inst.entities) {
        //        auto& meta = ecs.GetComponent<ECS::Component::EntityMeta>(e);
        //        uint64_t localId = meta.prefabLocalID;
        //        auto it = localToJson.find(localId);
        //        if (it == localToJson.end())
        //            continue;

        //        const auto& entVal = *it->second;

        //        //Serialization::JsonSceneSerializer::ReloadComponentsForEntity(*s_scene,
        //        //    e,
        //        //    inst.rootEntity,
        //        //    entVal);

        //        //if (ecs.HasComponent<Transform>(e) && e != inst.rootEntity) {
        //        //    auto& t = ecs.GetComponent<Transform>(e);
        //        //    t.isDirty = true;
        //        //}
        //    }
        //}
    }

    void PrefabManager::RebuildFromScene() {
        //if (!s_scene)
        //    return;

        //s_instances.clear();
        //s_entityToInstance.clear();
        //s_nextInstanceId = 1;

        //auto& ecs = s_scene->GetECSCoordinator();
        //const auto& allEntities = ecs.GetUsedEntities();

        //using NE::ECS::Component::EntityMeta;
        //using NE::ECS::Component::Transform;

        //std::unordered_set<uint32_t> visited;
        //visited.reserve(allEntities.size());

        //for (uint32_t e : allEntities) {
        //    auto& metaRoot = ecs.GetComponent<EntityMeta>(e);
        //    if (metaRoot.prefabID.empty() || !metaRoot.isPrefabRoot)
        //        continue;

        //    InstanceInfo info{};
        //    info.instanceId = s_nextInstanceId++;
        //    info.prefabAsset = metaRoot.prefabID;
        //    info.rootEntity = e;

        //    std::vector<uint32_t> stack;
        //    stack.push_back(e);

        //    while (!stack.empty()) {
        //        uint32_t cur = stack.back();
        //        stack.pop_back();

        //        if (!visited.insert(cur).second)
        //            continue;

        //        info.entities.push_back(cur);
        //        s_entityToInstance[cur] = info.instanceId;

        //        if (!ecs.HasComponent<EntityMeta>(cur))
        //            continue;

        //        auto& meta = ecs.GetComponent<EntityMeta>(cur);

        //        meta.prefabID = metaRoot.prefabID;
        //        meta.prefabInstanceID = info.instanceId;
        //        meta.isPrefabRoot = (cur == e);

        //        if (!ecs.HasComponent<Transform>(cur))
        //            continue;

        //        auto& t = ecs.GetComponent<Transform>(cur);
        //        //for (uint32_t child : t.children) {
        //        //    if (!ecs.HasComponent<EntityMeta>(child))
        //        //        continue;

        //        //    auto& childMeta = ecs.GetComponent<EntityMeta>(child);
        //        //    if (childMeta.prefabID == metaRoot.prefabID)
        //        //        stack.push_back(child);
        //        //}
        //    }

        //    s_instances[info.instanceId] = std::move(info);
        //}
    }

    //uint64_t PrefabManager::GetInstanceId(uint32_t entity) {
    //    auto it = s_entityToInstance.find(entity);
    //    return (it == s_entityToInstance.end()) ? 0 : it->second;
    //}

    //uint32_t PrefabManager::GetRootOfInstance(uint64_t instanceId) {
    //    auto it = s_instances.find(instanceId);
    //    return (it == s_instances.end()) ? NE::ECS::NO_ENTITY : it->second.rootEntity;
    //}
}