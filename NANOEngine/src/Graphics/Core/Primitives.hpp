#pragma once

#include <memory>
#include "Model.hpp"

namespace NANOEngine::Graphics {

    std::shared_ptr<Model> CreateCube(float width = 1.f,
        float height = 1.f,
        float depth = 1.f);

    std::shared_ptr<Model> CreatePlane(float width = 1.f,
        float depth = 1.f);

    std::shared_ptr<Model> CreateCylinder(float radius = 1.f,
        float height = 1.f,
        int segments = 20);

}
