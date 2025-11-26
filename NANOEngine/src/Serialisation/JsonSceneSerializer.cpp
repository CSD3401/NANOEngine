#include "JsonSceneSerializer.hpp"
#include "ReflectionJson.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Core/ECSCoordinator.hpp"

// Components
#include "../ECS/Components/EntityMeta.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../ECS/Components/Light.hpp"
#include "../ECS/Components/Collider.hpp"
#include "../ECS/Components/Rigidbody.hpp"
#include "../ECS/Components/NativeScript.hpp"
#include "ECS/Components/Camera.hpp"

#include "../Graphics/Core/Model.hpp"
#include "../ECS/Components/ComponentKey.hpp"

#include "ECS/Systems/ScriptSystem.hpp"
#include "Core/LUIDGenerator.hpp"
#include "ResourceManagement/ResourceManager.hpp"
// rapidjson
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/Core/RenderSettings.hpp"
#include <fstream>
#include <functional>

namespace {
	// RAII helper to automatically set and restore serialization context
	class SerializationContextGuard {
	public:
		SerializationContextGuard(NE::ECS::ECSCoordinator& ecs) {
			NE::Serialization::g_serializationContext.componentManager = &ecs.GetComponentManager();
			NE::Serialization::g_serializationContext.entityManager = &ecs.GetEntityManager();
		}
		~SerializationContextGuard() {
			NE::Serialization::g_serializationContext.componentManager = nullptr;
			NE::Serialization::g_serializationContext.entityManager = nullptr;
		}
	};

	template <typename C>
	void WriteComponentIfPresent(NE::ECS::ECSCoordinator& ecs, NE::ECS::Entity e,
		rapidjson::Value& ent, rapidjson::Document::AllocatorType& a) {
		if (!ecs.HasComponent<C>(e)) return;
		auto& c = ecs.GetComponent<C>(e);
		ent.AddMember(rapidjson::Value(ComponentKey<C>::value, a), NE::Serialization::to_json(c, a), a);
	}

	template <typename C>
	void ReadComponentIfPresent(NE::ECS::ECSCoordinator& ecs, NE::ECS::Entity e,
		const rapidjson::Value& ent) {
		if (!ent.HasMember(ComponentKey<C>::value)) return;
		C c{};
		NE::Serialization::from_json(ent[ComponentKey<C>::value], c);
		ecs.AddComponent(e, c);
	}

	template <typename C>
	void ReloadComponent(NE::ECS::ECSCoordinator& ecs,
		NE::ECS::Entity e,
		const rapidjson::Value& ent)
	{
		constexpr auto* key = ComponentKey<C>::value;

		const bool hasInJson = ent.HasMember(key);
		const bool hasOnEntity = ecs.HasComponent<C>(e);

		if (!hasInJson) {
			if (hasOnEntity) {
				ecs.RemoveComponent<C>(e);
			}
			return;
		}

		const auto& jComp = ent[key];

		// ------------------------------------------------------------
		// SPECIAL CASE: TRANSFORM
		// ------------------------------------------------------------
		if constexpr (std::is_same_v<C, NE::ECS::Component::Transform>) {
			//if (!hasOnEntity) {
			//    // create missing transform (rare)
			//    C newT{};
			//    from_json(jComp, newT);
			//    // sanitize:
			//    newT.luid = ecs.GetComponent<C>(e).luid; // preserve existing luid
			//    newT.parentLuid = ecs.GetComponent<C>(e).parentLuid; // preserve
			//    newT.parent = ecs.GetComponent<C>(e).parent;
			//    newT.children = ecs.GetComponent<C>(e).children;
			//    newT.localMatrix.SetToIdentity();
			//    newT.worldMatrix.SetToIdentity();
			//    newT.isDirty = true;
			//    ecs.AddComponent<C>(e, newT);
			//} else {
			auto& t = ecs.GetComponent<C>(e);

			C temp{};
			NE::Serialization::from_json(jComp, temp);

			// Only copy user-editable fields:
			t.localPosition = temp.localPosition;
			t.localRotationEuler = temp.localRotationEuler;
			t.localScale = temp.localScale;

			// DO NOT TOUCH:
			//  - t.luid
			//  - t.parentLuid
			//  - t.parent
			//  - t.children
			//  - matrices
			//t.isDirty = true;
			//}

			return;
		}

		if (hasOnEntity) {
			auto& comp = ecs.GetComponent<C>(e);
			NE::Serialization::from_json(jComp, comp);
		} else {
			C c{};
			NE::Serialization::from_json(jComp, c);
			ecs.AddComponent<C>(e, c);
		}
	}

	using ComponentTypes = std::tuple<
		NE::ECS::Component::EntityMeta,
		NE::ECS::Component::Transform,
		NE::ECS::Component::Renderer,
		NE::ECS::Component::Light,
		NE::ECS::Component::Collider,
		NE::ECS::Component::Rigidbody,
		NE::ECS::Component::NativeScript,
		NE::ECS::Component::Camera
	>;

	template <class F>
	void ForEachComponentType(F&& f) {
		std::apply([&](auto&&... t) {
			(f.template operator() < std::decay_t<decltype(t)> > (), ...);
			}, ComponentTypes{});
	}

	static void CollectPrefabSubtree(NE::ECS::ECSCoordinator& ecs,
		uint32_t root,
		std::vector<uint32_t>& out)
	{
		using NE::ECS::Component::Transform;

		if (!ecs.HasComponent<Transform>(root))
			return;

		out.push_back(root);

		auto& t = ecs.GetComponent<Transform>(root);
		for (uint32_t child : t.children) {
			// Defensive: skip invalid / missing transform children
			if (!ecs.HasComponent<Transform>(child))
				continue;

			CollectPrefabSubtree(ecs, child, out);
		}
	}
}

namespace NE::Serialization {
	using namespace rapidjson;

	// phase out next time
	void JsonSceneSerializer::Serialize(SceneManagement::Scene& scene, const std::string& path) {
		SerializationContextGuard guard(scene.GetECSCoordinator());

		Document doc; doc.SetObject(); auto& a = doc.GetAllocator();

		auto& rs = Graphics::GraphicsManager::renderSettings;
		doc.AddMember("RenderSettings", NE::Serialization::to_json(rs, a), a);

		Value entities(kArrayType);

		const auto& ids = scene.GetECSCoordinator().GetUsedEntities();
		for (NE::ECS::Entity e : ids) {
			Value ent(kObjectType);

			// Iterate the registered component types
			ForEachComponentType([&]<typename C>() {
				WriteComponentIfPresent<C>(scene.GetECSCoordinator(), e, ent, a);
			});

			entities.PushBack(ent, a);
		}
		doc.AddMember("Entities", entities, a);

		rapidjson::StringBuffer sb;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> wr(sb);
		doc.Accept(wr);
		std::ofstream out(path, std::ios::binary);
		if (out) out.write(sb.GetString(), static_cast<std::streamsize>(sb.GetSize()));
	}

	void JsonSceneSerializer::Serialize(SceneManagement::Scene& scene, std::vector<uint32_t>& hierarchy, const std::string& path) {
		SerializationContextGuard guard(scene.GetECSCoordinator());

		Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
		Value entities(kArrayType);

		//const auto& ids = hierarchy;
		for (NE::ECS::Entity e : hierarchy) {
			Value ent(kObjectType);

			// Iterate the registered component types
			ForEachComponentType([&]<typename C>() {
				WriteComponentIfPresent<C>(scene.GetECSCoordinator(), e, ent, a);
			});

			entities.PushBack(ent, a);
		}
		doc.AddMember("Entities", entities, a);

		rapidjson::StringBuffer sb;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> wr(sb);
		doc.Accept(wr);
		std::ofstream out(path, std::ios::binary);
		if (out) out.write(sb.GetString(), static_cast<std::streamsize>(sb.GetSize()));
	}

	void JsonSceneSerializer::Deserialize(SceneManagement::Scene& scene, const std::string& path) {
		SerializationContextGuard guard(scene.GetECSCoordinator());

		std::ifstream in(path, std::ios::binary);
		if (!in) return;

		std::string data((std::istreambuf_iterator<char>(in)), {});
		Document doc; doc.Parse(data.c_str());

		if (doc.HasMember("RenderSettings")) {
			auto& rs = Graphics::GraphicsManager::renderSettings;
			NE::Serialization::from_json(doc["RenderSettings"], rs);
		}

		if (!doc.IsObject() || !doc.HasMember("Entities")) return;

		for (auto& entVal : doc["Entities"].GetArray()) {
			NE::ECS::Entity e = scene.GetECSCoordinator().CreateEntity();

			ForEachComponentType([&]<typename C>() {
				ReadComponentIfPresent<C>(scene.GetECSCoordinator(), e, entVal);
			});
		}
	}

	std::string JsonSceneSerializer::SerializePrefab(SceneManagement::Scene& scene, uint32_t rootEnt, const std::string& directoryPath) {
		using namespace rapidjson;
		auto& ecs = scene.GetECSCoordinator();

		using NE::ECS::Component::EntityMeta;
		using NE::ECS::Component::Transform;

		if (!ecs.HasComponent<Transform>(rootEnt))
			return "";

		std::vector<uint32_t> entities;
		entities.reserve(16);
		CollectPrefabSubtree(ecs, rootEnt, entities);

		if (entities.empty())
			return "";

		std::ofstream out;

		std::unordered_map<uint32_t, uint64_t> entityToLocalId;
		entityToLocalId.reserve(entities.size());

		uint64_t nextId = 1;
		for (uint32_t e : entities) {
			entityToLocalId[e] = nextId++;
		}

		Document doc;
		doc.SetObject();
		auto& a = doc.GetAllocator();
		Value entitiesArr(kArrayType);

		for (uint32_t e : entities) {
			Value ent(kObjectType);

			ForEachComponentType([&]<typename C>() {
				WriteComponentIfPresent<C>(ecs, e, ent, a);
			});

			if (ent.HasMember(ComponentKey<EntityMeta>::value)) {
				auto& eJson = ent[ComponentKey<EntityMeta>::value];
				const auto& eM = ecs.GetComponent<EntityMeta>(e);

				const uint64_t myId = entityToLocalId[e];

				if (eJson.HasMember("prefabLocalID"))
					eJson["prefabLocalID"].SetUint64(myId);
				else
					eJson.AddMember("prefabLocalID", myId, a);
			}

			if (ent.HasMember(ComponentKey<Transform>::value)) {
				auto& tJson = ent[ComponentKey<Transform>::value];
				const auto& t = ecs.GetComponent<Transform>(e);

				const uint64_t myId = entityToLocalId[e];

				uint64_t parentId = 0;
				if (t.parent != NE::ECS::Component::INVALID_PARENT) {
					auto it = entityToLocalId.find(t.parent);
					if (it != entityToLocalId.end())
						parentId = it->second;
				}

				if (tJson.HasMember("luid"))
					tJson["luid"].SetUint64(myId);
				else
					tJson.AddMember("luid", myId, a);

				if (tJson.HasMember("parentLuid"))
					tJson["parentLuid"].SetUint64(parentId);
				else
					tJson.AddMember("parentLuid", parentId, a);
			}

			entitiesArr.PushBack(ent, a);
		}

		doc.AddMember("Entities", entitiesArr, a);

		rapidjson::StringBuffer sb;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> wr(sb);
		doc.Accept(wr);

		out.open(directoryPath, std::ios::binary | std::ios::in | std::ios::out);
		if (out) {
			out.write(sb.GetString(), static_cast<std::streamsize>(sb.GetSize()));
		}

		return directoryPath;
	}

	std::vector<uint32_t> JsonSceneSerializer::DeserializePrefab(SceneManagement::Scene& scene, const std::string& path) {
		std::vector<uint32_t> ret{};
		std::ifstream in(path, std::ios::binary);
		if (!in) return ret;

		std::string data((std::istreambuf_iterator<char>(in)), {});
		Document doc;
		doc.Parse(data.c_str());
		if (!doc.IsObject() || !doc.HasMember("Entities"))
			return ret;

		auto& ecs = scene.GetECSCoordinator();
		auto entities = doc["Entities"].GetArray();
		const size_t count = entities.Size();

		using NE::ECS::Component::Transform;

		std::vector<NE::ECS::Entity> created(count, NE::ECS::NO_ENTITY);
		std::vector<uint64_t> prefabLuid(count, 0);
		std::vector<uint64_t> prefabParentLuid(count, 0);
		std::vector<bool> hasTransform(count, false);

		for (size_t i = 0; i < count; ++i) {
			auto& entVal = entities[i];

			if (entVal.HasMember(ComponentKey<Transform>::value)) {
				auto& tJson = entVal[ComponentKey<Transform>::value];

				if (tJson.HasMember("luid") && tJson["luid"].IsUint64())
					prefabLuid[i] = tJson["luid"].GetUint64();

				if (tJson.HasMember("parentLuid") && tJson["parentLuid"].IsUint64())
					prefabParentLuid[i] = tJson["parentLuid"].GetUint64();

				hasTransform[i] = true;

				uint64_t newLuid = Core::LUIDGenerator::Generate("tr");
				tJson["luid"].SetUint64(newLuid);

				tJson["parentLuid"].SetUint64(0);
			}

			NE::ECS::Entity e = ecs.CreateEntity();
			ret.push_back(e);
			created[i] = e;

			ForEachComponentType([&]<typename C>() {
				ReadComponentIfPresent<C>(ecs, e, entVal);
			});
		}

		std::unordered_map<uint64_t, NE::ECS::Entity> prefabToEntity;
		prefabToEntity.reserve(count);

		for (size_t i = 0; i < count; ++i) {
			if (!hasTransform[i]) continue;
			if (prefabLuid[i] == 0) continue;
			prefabToEntity[prefabLuid[i]] = created[i];
		}

		for (size_t i = 0; i < count; ++i) {
			if (!hasTransform[i]) continue;

			uint64_t parentLocal = prefabParentLuid[i];
			if (parentLocal == 0)
				continue;

			auto it = prefabToEntity.find(parentLocal);
			if (it == prefabToEntity.end())
				continue;

			NE::ECS::Entity child = created[i];
			NE::ECS::Entity parent = it->second;

			auto& childT = ecs.GetComponent<Transform>(child);
			auto& parentT = ecs.GetComponent<Transform>(parent);

			if (childT.parent != NE::ECS::Component::INVALID_PARENT) {
				auto& oldParentT = ecs.GetComponent<Transform>(childT.parent);
				auto& vec = oldParentT.children;
				vec.erase(std::remove(vec.begin(), vec.end(), child), vec.end());
			}

			childT.parent = parent;
			parentT.children.push_back(child);
			childT.parentLuid = parentT.luid;

			childT.isDirty = true;
		}

		return ret;
	}

	std::vector<uint32_t> JsonSceneSerializer::DeserializePrefab(SceneManagement::Scene& scene, const std::string& path, NE::Math::Vec3 camForwardPos) {
		std::vector<uint32_t> ret{};
		std::ifstream in(path, std::ios::binary);
		if (!in) return ret;

		std::string data((std::istreambuf_iterator<char>(in)), {});
		Document doc;
		doc.Parse(data.c_str());
		if (!doc.IsObject() || !doc.HasMember("Entities"))
			return ret;

		auto& ecs = scene.GetECSCoordinator();
		auto entities = doc["Entities"].GetArray();
		const size_t count = entities.Size();

		using NE::ECS::Component::Transform;

		std::vector<NE::ECS::Entity> created(count, NE::ECS::NO_ENTITY);
		std::vector<uint64_t> prefabLuid(count, 0);
		std::vector<uint64_t> prefabParentLuid(count, 0);
		std::vector<bool> hasTransform(count, false);

		for (size_t i = 0; i < count; ++i) {
			auto& entVal = entities[i];

			if (entVal.HasMember(ComponentKey<Transform>::value)) {
				auto& tJson = entVal[ComponentKey<Transform>::value];

				if (tJson.HasMember("luid") && tJson["luid"].IsUint64())
					prefabLuid[i] = tJson["luid"].GetUint64();

				if (tJson.HasMember("parentLuid") && tJson["parentLuid"].IsUint64())
					prefabParentLuid[i] = tJson["parentLuid"].GetUint64();

				hasTransform[i] = true;

				uint64_t newLuid = Core::LUIDGenerator::Generate("tr");
				tJson["luid"].SetUint64(newLuid);

				tJson["parentLuid"].SetUint64(0);

				if (prefabParentLuid[i] == 0) {
					tJson["Position"]["x"].SetFloat(camForwardPos.x);
					tJson["Position"]["y"].SetFloat(camForwardPos.y);
					tJson["Position"]["z"].SetFloat(camForwardPos.z);
				}
			}

			NE::ECS::Entity e = ecs.CreateEntity();
			ret.push_back(e);
			created[i] = e;

			ForEachComponentType([&]<typename C>() {
				ReadComponentIfPresent<C>(ecs, e, entVal);
			});
		}

		std::unordered_map<uint64_t, NE::ECS::Entity> prefabToEntity;
		prefabToEntity.reserve(count);

		for (size_t i = 0; i < count; ++i) {
			if (!hasTransform[i]) continue;
			if (prefabLuid[i] == 0) continue;
			prefabToEntity[prefabLuid[i]] = created[i];
		}

		for (size_t i = 0; i < count; ++i) {
			if (!hasTransform[i]) continue;

			uint64_t parentLocal = prefabParentLuid[i];
			if (parentLocal == 0)
				continue;

			auto it = prefabToEntity.find(parentLocal);
			if (it == prefabToEntity.end())
				continue;

			NE::ECS::Entity child = created[i];
			NE::ECS::Entity parent = it->second;

			auto& childT = ecs.GetComponent<Transform>(child);
			auto& parentT = ecs.GetComponent<Transform>(parent);

			if (childT.parent != NE::ECS::Component::INVALID_PARENT) {
				auto& oldParentT = ecs.GetComponent<Transform>(childT.parent);
				auto& vec = oldParentT.children;
				vec.erase(std::remove(vec.begin(), vec.end(), child), vec.end());
			}

			childT.parent = parent;
			parentT.children.push_back(child);
			childT.parentLuid = parentT.luid;

			childT.isDirty = true;
		}

		return ret;
	}

	void JsonSceneSerializer::ReloadComponentsForEntity(SceneManagement::Scene& scene,
		uint32_t entity,
		uint32_t rootEntity,
		const rapidjson::Value& entVal)
	{
		auto& ecs = scene.GetECSCoordinator();

		ForEachComponentType([&]<typename C>() {
			// SKIP RULE: skip Transform on root entity (to preserve instance placement)
			if constexpr (std::is_same_v<C, NE::ECS::Component::Transform>) {
				if (entity == rootEntity)
					return;
			}

			ReloadComponent<C>(ecs, entity, entVal);
		});

		// Optional: after reloading, mark Transform dirty
		if (ecs.HasComponent<NE::ECS::Component::Transform>(entity)
			&& entity != rootEntity) {
			auto& t = ecs.GetComponent<NE::ECS::Component::Transform>(entity);
			t.isDirty = true;
		}
		if (ecs.HasComponent<NE::ECS::Component::Renderer>(entity)) {
			auto& renderer = ecs.GetComponent<NE::ECS::Component::Renderer>(entity);

			if (!renderer.materialUUID.empty())
				renderer.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>(renderer.materialUUID);
			if (!renderer.modelUUID.empty())
				renderer.model = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Model>(renderer.modelUUID);
		}
	}

	void JsonSceneSerializer::SerializePrefabToMemory(SceneManagement::Scene& scene,
		uint32_t rootEnt,
		std::vector<uint8_t>& outBuffer)
	{
		using namespace rapidjson;
		auto& ecs = scene.GetECSCoordinator();

		using NE::ECS::Component::EntityMeta;
		using NE::ECS::Component::Transform;

		if (!ecs.HasComponent<Transform>(rootEnt))
			return;

		std::vector<uint32_t> entities;
		entities.reserve(16);
		CollectPrefabSubtree(ecs, rootEnt, entities);

		if (entities.empty())
			return;

		std::unordered_map<uint32_t, uint64_t> entityToLocalId;
		entityToLocalId.reserve(entities.size());

		uint64_t nextId = 1;
		for (uint32_t e : entities) {
			entityToLocalId[e] = nextId++;
		}

		Document doc;
		doc.SetObject();
		auto& a = doc.GetAllocator();
		Value entitiesArr(kArrayType);

		for (uint32_t e : entities) {
			Value ent(kObjectType);

			ForEachComponentType([&]<typename C>() {
				WriteComponentIfPresent<C>(ecs, e, ent, a);
			});

			if (ent.HasMember(ComponentKey<EntityMeta>::value)) {
				auto& eJson = ent[ComponentKey<EntityMeta>::value];
				const uint64_t myId = entityToLocalId[e];

				if (eJson.HasMember("prefabLocalID"))
					eJson["prefabLocalID"].SetUint64(myId);
				else
					eJson.AddMember("prefabLocalID", myId, a);
			}

			if (ent.HasMember(ComponentKey<Transform>::value)) {
				auto& tJson = ent[ComponentKey<Transform>::value];
				const auto& t = ecs.GetComponent<Transform>(e);

				const uint64_t myId = entityToLocalId[e];

				uint64_t parentId = 0;
				if (t.parent != NE::ECS::Component::INVALID_PARENT) {
					auto it = entityToLocalId.find(t.parent);
					if (it != entityToLocalId.end())
						parentId = it->second;
				}

				if (tJson.HasMember("luid"))
					tJson["luid"].SetUint64(myId);
				else
					tJson.AddMember("luid", myId, a);

				if (tJson.HasMember("parentLuid"))
					tJson["parentLuid"].SetUint64(parentId);
				else
					tJson.AddMember("parentLuid", parentId, a);
			}

			entitiesArr.PushBack(ent, a);
		}

		doc.AddMember("Entities", entitiesArr, a);

		rapidjson::StringBuffer sb;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> wr(sb);
		doc.Accept(wr);

		outBuffer.clear();
		outBuffer.resize(sb.GetSize());
		std::memcpy(outBuffer.data(), sb.GetString(), sb.GetSize());
	}

	std::vector<uint32_t> JsonSceneSerializer::DeserializePrefabFromMemory(SceneManagement::Scene& scene,
		const uint8_t* data,
		size_t size)
	{
		std::vector<uint32_t> ret{};
		if (!data || size == 0)
			return ret;

		using namespace rapidjson;

		std::string jsonStr(reinterpret_cast<const char*>(data), size);
		Document doc;
		doc.Parse(jsonStr.c_str());
		if (!doc.IsObject() || !doc.HasMember("Entities"))
			return ret;

		auto& ecs = scene.GetECSCoordinator();
		auto entities = doc["Entities"].GetArray();
		const size_t count = entities.Size();

		using NE::ECS::Component::Transform;

		std::vector<NE::ECS::Entity> created(count, NE::ECS::NO_ENTITY);
		std::vector<uint64_t> prefabLuid(count, 0);
		std::vector<uint64_t> prefabParentLuid(count, 0);
		std::vector<bool> hasTransform(count, false);

		for (size_t i = 0; i < count; ++i) {
			auto& entVal = entities[i];

			if (entVal.HasMember(ComponentKey<Transform>::value)) {
				auto& tJson = entVal[ComponentKey<Transform>::value];

				if (tJson.HasMember("luid") && tJson["luid"].IsUint64())
					prefabLuid[i] = tJson["luid"].GetUint64();

				if (tJson.HasMember("parentLuid") && tJson["parentLuid"].IsUint64())
					prefabParentLuid[i] = tJson["parentLuid"].GetUint64();

				hasTransform[i] = true;

				uint64_t newLuid = Core::LUIDGenerator::Generate("tr");
				tJson["luid"].SetUint64(newLuid);
				tJson["parentLuid"].SetUint64(0);
			}

			NE::ECS::Entity e = ecs.CreateEntity();
			ret.push_back(e);
			created[i] = e;

			ForEachComponentType([&]<typename C>() {
				ReadComponentIfPresent<C>(ecs, e, entVal);
			});
		}

		std::unordered_map<uint64_t, NE::ECS::Entity> prefabToEntity;
		prefabToEntity.reserve(count);

		for (size_t i = 0; i < count; ++i) {
			if (!hasTransform[i]) continue;
			if (prefabLuid[i] == 0) continue;
			prefabToEntity[prefabLuid[i]] = created[i];
		}

		for (size_t i = 0; i < count; ++i) {
			if (!hasTransform[i]) continue;

			uint64_t parentLocal = prefabParentLuid[i];
			if (parentLocal == 0)
				continue;

			auto it = prefabToEntity.find(parentLocal);
			if (it == prefabToEntity.end())
				continue;

			NE::ECS::Entity child = created[i];
			NE::ECS::Entity parent = it->second;

			auto& childT = ecs.GetComponent<Transform>(child);
			auto& parentT = ecs.GetComponent<Transform>(parent);

			if (childT.parent != NE::ECS::Component::INVALID_PARENT) {
				auto& oldParentT = ecs.GetComponent<Transform>(childT.parent);
				auto& vec = oldParentT.children;
				vec.erase(std::remove(vec.begin(), vec.end(), child), vec.end());
			}

			childT.parent = parent;
			parentT.children.push_back(child);
			childT.parentLuid = parentT.luid;
			childT.isDirty = true;
		}

		return ret;
	}

	std::vector<uint32_t> JsonSceneSerializer::DeserializePrefabFromMemory(SceneManagement::Scene& scene,
		const std::vector<uint8_t>& buffer)
	{
		return DeserializePrefabFromMemory(scene, buffer.data(), buffer.size());
	}

	void JsonSceneSerializer::SerializeToMemory(SceneManagement::Scene& scene, std::vector<uint8_t>& outBuffer) {
		SerializationContextGuard guard(scene.GetECSCoordinator());

		Document doc;
		doc.SetObject();
		auto& a = doc.GetAllocator();
		Value entities(kArrayType);

		const auto& ids = scene.GetECSCoordinator().GetUsedEntities();
		for (NE::ECS::Entity e : ids) {
			Value ent(kObjectType);

			ForEachComponentType([&]<typename C>() {
				WriteComponentIfPresent<C>(scene.GetECSCoordinator(), e, ent, a);
			});

			entities.PushBack(ent, a);
		}
		doc.AddMember("Entities", entities, a);

		rapidjson::StringBuffer sb;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> wr(sb);
		doc.Accept(wr);

		outBuffer.clear();
		outBuffer.resize(sb.GetSize());
		std::memcpy(outBuffer.data(), sb.GetString(), sb.GetSize());
	}

	void JsonSceneSerializer::DeserializeFromMemory(SceneManagement::Scene& scene, const uint8_t* data, size_t size) {
		SerializationContextGuard guard(scene.GetECSCoordinator());

		if (!data || size == 0) return;

		std::string jsonStr(reinterpret_cast<const char*>(data), size);

		Document doc;
		doc.Parse(jsonStr.c_str());
		if (!doc.IsObject() || !doc.HasMember("Entities"))
			return;

		for (auto& entVal : doc["Entities"].GetArray()) {
			NE::ECS::Entity e = scene.GetECSCoordinator().CreateEntity();

			ForEachComponentType([&]<typename C>() {
				ReadComponentIfPresent<C>(scene.GetECSCoordinator(), e, entVal);
			});
		}
	}

	void JsonSceneSerializer::DeserializeFromMemory(SceneManagement::Scene& scene, const std::vector<uint8_t>& buffer) {
		DeserializeFromMemory(scene, buffer.data(), buffer.size());
	}
}