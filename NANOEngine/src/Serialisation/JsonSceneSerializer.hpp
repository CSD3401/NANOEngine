#pragma once
#include "ISceneSerializer.hpp"
#include <rapidjson/document.h>

namespace NANOEngine::Serialization {
    class JsonSceneSerializer {
    public:
        static void Serialize(SceneManagement::Scene& scene, const std::string& path);
        static void Deserialize(SceneManagement::Scene& scene, const std::string& path);
    };
}