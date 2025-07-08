#include "JsonSceneSerializer.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Core/ECSCoordinator.hpp"

// Components
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../ECS/Components/Light.hpp"
#include "../Graphics/Core/Model.hpp"

// rapidjson
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>

namespace NANOEngine::Serialization {

    using namespace rapidjson;

    static Value Vec3ToJson(const Math::Vec3& v, Document::AllocatorType& a) {
        Value obj(kObjectType);
        obj.AddMember("x", v.x, a);
        obj.AddMember("y", v.y, a);
        obj.AddMember("z", v.z, a);
        return obj;
    }

    static Math::Vec3 JsonToVec3(const Value& v) {
        Math::Vec3 out;
        if (v.HasMember("x")) out.x = v["x"].GetFloat();
        if (v.HasMember("y")) out.y = v["y"].GetFloat();
        if (v.HasMember("z")) out.z = v["z"].GetFloat();
        return out;
    }

    void JsonSceneSerializer::Serialize(SceneManagement::Scene& scene, const std::string& path) {
        Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();
        Value entities(kArrayType);

        const auto& ids = scene.GetECSCoordinator().GetUsedEntities();
        for (ECS::Entity e : ids) {
            Value ent(kObjectType);
            if (scene.GetECSCoordinator().HasComponent<ECS::Component::Transform>(e)) {
                const auto& t = scene.GetECSCoordinator().GetComponent<ECS::Component::Transform>(e);
                Value tObj(kObjectType);
                tObj.AddMember("position", Vec3ToJson(t.position, allocator), allocator);
                tObj.AddMember("scale", Vec3ToJson(t.scale, allocator), allocator);
                tObj.AddMember("rotation", Vec3ToJson(t.rotation, allocator), allocator);
                ent.AddMember("Transform", tObj, allocator);
            }
            if (scene.GetECSCoordinator().HasComponent<ECS::Component::Renderer>(e)) {
                const auto& r = scene.GetECSCoordinator().GetComponent<ECS::Component::Renderer>(e);
                Value rObj(kObjectType);
                rObj.AddMember("modelPath", Value(r.modelPath.string().c_str(), allocator), allocator);
                ent.AddMember("Renderer", rObj, allocator);
            }
            if (scene.GetECSCoordinator().HasComponent<ECS::Component::Light>(e)) {
                const auto& l = scene.GetECSCoordinator().GetComponent<ECS::Component::Light>(e);
                Value lObj(kObjectType);
                lObj.AddMember("type", static_cast<int>(l.type), allocator);
                lObj.AddMember("direction", Vec3ToJson(l.direction, allocator), allocator);
                lObj.AddMember("color", Vec3ToJson(l.color, allocator), allocator);
                lObj.AddMember("intensity", l.intensity, allocator);
                lObj.AddMember("innerCutoff", l.innerCutoff, allocator);
                lObj.AddMember("outerCutoff", l.outerCutoff, allocator);
                lObj.AddMember("constant", l.constant, allocator);
                lObj.AddMember("linear", l.linear, allocator);
                lObj.AddMember("quadratic", l.quadratic, allocator);

                ent.AddMember("Light", lObj, allocator);
            }
            entities.PushBack(ent, allocator);
        }
        doc.AddMember("Entities", entities, allocator);

        StringBuffer buffer;
        PrettyWriter<StringBuffer> writer(buffer);
        doc.Accept(writer);
        std::ofstream out(path);
        if (out.is_open())
            out << buffer.GetString();
    }

    void JsonSceneSerializer::Deserialize(SceneManagement::Scene& scene, const std::string& path) {
        std::ifstream in(path);
        if (!in.is_open()) return;
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        Document doc;
        doc.Parse(data.c_str());
        if (!doc.IsObject() || !doc.HasMember("Entities")) return;

        for (auto& entVal : doc["Entities"].GetArray()) {
            ECS::Entity e = scene.GetECSCoordinator().CreateEntity();
            if (entVal.HasMember("Transform")) {
                ECS::Component::Transform t{};
                const auto& tVal = entVal["Transform"];
                if (tVal.HasMember("position")) t.position = JsonToVec3(tVal["position"]);
                if (tVal.HasMember("scale")) t.scale = JsonToVec3(tVal["scale"]);
                if (tVal.HasMember("rotation")) t.rotation = JsonToVec3(tVal["rotation"]);
                t.isDirty = true;
                scene.GetECSCoordinator().AddComponent(e, t);
            }
            if (entVal.HasMember("Renderer")) {
                ECS::Component::Renderer r{};
                const auto& rVal = entVal["Renderer"];
                if (rVal.HasMember("modelPath")) {
                    r.modelPath = rVal["modelPath"].GetString();
                    if (!r.modelPath.empty())
                        r.model = Graphics::LoadModel(r.modelPath.string());

                    scene.GetECSCoordinator().AddComponent(e, r);
                }
            }
            if (entVal.HasMember("Light")) {
                ECS::Component::Light l{};
                const auto& lVal = entVal["Light"];

                if (lVal.HasMember("type")) l.type = static_cast<ECS::Component::Light::Type>(lVal["type"].GetInt());
                if (lVal.HasMember("direction")) l.direction = JsonToVec3(lVal["direction"]);
                if (lVal.HasMember("color")) l.color = JsonToVec3(lVal["color"]);
                if (lVal.HasMember("intensity")) l.intensity = lVal["intensity"].GetFloat();
                if (lVal.HasMember("innerCutoff")) l.innerCutoff = lVal["innerCutoff"].GetFloat();
                if (lVal.HasMember("outerCutoff")) l.outerCutoff = lVal["outerCutoff"].GetFloat();
                if (lVal.HasMember("constant")) l.constant = lVal["constant"].GetFloat();
                if (lVal.HasMember("linear")) l.linear = lVal["linear"].GetFloat();
                if (lVal.HasMember("quadratic")) l.quadratic = lVal["quadratic"].GetFloat();

                scene.GetECSCoordinator().AddComponent(e, l);
            }
        }
    }
}