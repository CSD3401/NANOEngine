#pragma once
#include "ISceneSerializer.hpp"

namespace NE::Serialization {
    class JsonSceneSerializer {
    public:
        static void Serialize(SceneManagement::Scene& scene, const std::string& path);
        static void Deserialize(SceneManagement::Scene& scene, const std::string& path);
    };
}