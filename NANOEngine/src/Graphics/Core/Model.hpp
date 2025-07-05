#pragma once

#include <memory>
#include <vector>
#include <string>
#include "Vertex.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"

namespace NANOEngine::Graphics {

    struct SubMesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::shared_ptr<IGeometryBuffer> buffer;
    };

    class Model {
    public:
        std::vector<SubMesh> meshes;
    };

    std::shared_ptr<Model> LoadModel(const std::string& path);

}