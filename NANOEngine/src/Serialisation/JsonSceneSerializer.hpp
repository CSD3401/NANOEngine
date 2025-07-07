#pragma once
#include "ISceneSerializer.hpp"
#include <rapidjson/document.h>

namespace NANOEngine::Serialization {
    class JsonSceneSerializer : public ISceneSerializer {
    public:
        void Serialize(const SceneManagement::Scene& scene, const std::string& path) override;
        void Deserialize(SceneManagement::Scene& scene, const std::string& path) override;
    };
}