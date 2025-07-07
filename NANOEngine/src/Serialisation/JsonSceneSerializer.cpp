#include "JsonSceneSerializer.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Core/ECSCoordinator.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "../Graphics/Core/Model.hpp"
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>

namespace NANOEngine::Serialization {

    using namespace rapidjson;

    //static Value Vec3ToJson(const Math::Vec3& v, Document::AllocatorType& a) {
    //    Value obj(kObjectType);
    //    obj.AddMember("x", v.x, a);
    //    obj.AddMember("y", v.y, a);
    //    obj.AddMember("z", v.z, a);
    //    return obj;
    //}

    //static Math::Vec3 JsonToVec3(const Value& v) {
    //    Math::Vec3 out;
    //    if (v.HasMember("x")) out.x = v["x"].GetFloat();
    //    if (v.HasMember("y")) out.y = v["y"].GetFloat();
    //    if (v.HasMember("z")) out.z = v["z"].GetFloat();
    //    return out;
    //}

    void JsonSceneSerializer::Serialize(const SceneManagement::Scene& scene, const std::string& path) {
        scene; path;
        //Document doc;
        //doc.SetObject();
        //auto& allocator = doc.GetAllocator();
        //Value entities(kArrayType);

        //const auto ids = scene.GetECSCoordinator().m_transformSystem->GetEntities();
        //for (ECS::Entity e : ids) {
        //    Value ent(kObjectType);
        //    ent.AddMember("id", e, allocator);
        //    if (scene.GetECSCoordinator().HasComponent<ECS::Component::Transform>(e)) {
        //        const auto& t = scene.GetECSCoordinator().GetComponent<ECS::Component::Transform>(e);
        //        Value tObj(kObjectType);
        //        tObj.AddMember("position", Vec3ToJson(t.position, allocator), allocator);
        //        tObj.AddMember("scale", Vec3ToJson(t.scale, allocator), allocator);
        //        tObj.AddMember("rotation", Vec3ToJson(t.rotation, allocator), allocator);
        //        ent.AddMember("Transform", tObj, allocator);
        //    }
        //    if (scene.GetECSCoordinator().HasComponent<ECS::Component::Renderer>(e)) {
        //        const auto& r = scene.GetECSCoordinator().GetComponent<ECS::Component::Renderer>(e);
        //        Value rObj(kObjectType);
        //        rObj.AddMember("modelPath", Value(r.modelPath.string().c_str(), allocator), allocator);
        //        ent.AddMember("Renderer", rObj, allocator);
        //    }
        //    entities.PushBack(ent, allocator);
        //}
        //doc.AddMember("entities", entities, allocator);

        //StringBuffer buffer;
        //PrettyWriter<StringBuffer> writer(buffer);
        //doc.Accept(writer);
        //std::ofstream out(path);
        //if (out.is_open())
        //    out << buffer.GetString();
    }

    void JsonSceneSerializer::Deserialize(SceneManagement::Scene& scene, const std::string& path) {
        scene; path;
        //std::ifstream in(path);
        //if (!in.is_open()) return;
        //std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        //Document doc;
        //doc.Parse(data.c_str());
        //if (!doc.IsObject() || !doc.HasMember("entities")) return;

        //for (auto& entVal : doc["entities"].GetArray()) {
        //    ECS::Entity e = scene.GetECSCoordinator().CreateEntity();
        //    if (entVal.HasMember("Transform")) {
        //        auto& t = scene.GetECSCoordinator().GetComponent<ECS::Component::Transform>(e);
        //        const auto& tVal = entVal["Transform"];
        //        if (tVal.HasMember("position")) t.position = JsonToVec3(tVal["position"]);
        //        if (tVal.HasMember("scale")) t.scale = JsonToVec3(tVal["scale"]);
        //        if (tVal.HasMember("rotation")) t.rotation = JsonToVec3(tVal["rotation"]);
        //        t.isDirty = true;
        //    }
        //    if (entVal.HasMember("Renderer")) {
        //        auto& r = scene.GetECSCoordinator().GetComponent<ECS::Component::Renderer>(e);
        //        const auto& rVal = entVal["Renderer"];
        //        if (rVal.HasMember("modelPath")) {
        //            r.modelPath = rVal["modelPath"].GetString();
        //            if (!r.modelPath.empty())
        //                r.model = Graphics::LoadModel(r.modelPath.string());
        //        }
        //    }
        //}
    }
}