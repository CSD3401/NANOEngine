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

// rapidjson
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
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
    void ReloadComponent(NE::ECS::ECSCoordinator& ecs, NE::ECS::Entity e,
        const rapidjson::Value& ent) {
        if (!ent.HasMember(ComponentKey<C>::value)) return;
        C c{};
        NE::Serialization::from_json(ent[ComponentKey<C>::value], c);
        ecs.GetComponent<C>(e) = c;
        //ecs.AddComponent(e, c);
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
}

namespace NE::Serialization {

    using namespace rapidjson;

    // phase out next time
    void JsonSceneSerializer::Serialize(SceneManagement::Scene& scene, const std::string& path) {
        SerializationContextGuard guard(scene.GetECSCoordinator());

        Document doc; doc.SetObject(); auto& a = doc.GetAllocator();
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
        if (!doc.IsObject() || !doc.HasMember("Entities")) return;

        for (auto& entVal : doc["Entities"].GetArray()) {
            NE::ECS::Entity e = scene.GetECSCoordinator().CreateEntity();

            ForEachComponentType([&]<typename C>() {
                ReadComponentIfPresent<C>(scene.GetECSCoordinator(), e, entVal);
            });
        }
    }

    void JsonSceneSerializer::ReloadScene(SceneManagement::Scene& scene, std::vector<uint32_t>& hierarchy, const std::string& path) {
        SerializationContextGuard guard(scene.GetECSCoordinator());

        std::ifstream in(path, std::ios::binary);
        if (!in) return;

        std::string data((std::istreambuf_iterator<char>(in)), {});
        Document doc; doc.Parse(data.c_str());
        if (!doc.IsObject() || !doc.HasMember("Entities")) return;

        int i = 0;
        for (auto& entVal : doc["Entities"].GetArray()) {
            //ECS::Entity e = scene.GetECSCoordinator().CreateEntity();
            NE::ECS::Entity e = hierarchy[i++];

            ForEachComponentType([&]<typename C>() {
                ReloadComponent<C>(scene.GetECSCoordinator(), e, entVal);
            });
        }
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

            // same component loop as your file serialize
            ForEachComponentType([&]<typename C>() {
                WriteComponentIfPresent<C>(scene.GetECSCoordinator(), e, ent, a);
            });

            entities.PushBack(ent, a);
        }
        doc.AddMember("Entities", entities, a);

        // write to string buffer
        rapidjson::StringBuffer sb;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> wr(sb);
        doc.Accept(wr);

        // copy to byte buffer
        outBuffer.clear();
        outBuffer.resize(sb.GetSize());
        std::memcpy(outBuffer.data(), sb.GetString(), sb.GetSize());
    }

    void JsonSceneSerializer::DeserializeFromMemory(SceneManagement::Scene& scene, const uint8_t* data, size_t size) {
        SerializationContextGuard guard(scene.GetECSCoordinator());

        if (!data || size == 0) return;

        // rapidjson expects a null-terminated string; we can copy into a std::string
        std::string jsonStr(reinterpret_cast<const char*>(data), size);

        Document doc;
        doc.Parse(jsonStr.c_str());
        if (!doc.IsObject() || !doc.HasMember("Entities"))
            return;

        // NOTE: this matches your existing Deserialize: it just creates entities and fills components.
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