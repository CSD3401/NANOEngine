#pragma once

#include <memory>
#include "Model.hpp"

namespace NE::Graphics {

    std::shared_ptr<Model> CreateCube(float width = 1.f,
        float height = 1.f,
        float depth = 1.f);

    std::shared_ptr<Model> CreatePlane(float width = 1.f,
        float depth = 1.f);

    std::shared_ptr<Model> CreateCylinder(float radius = 1.f,
        float height = 1.f,
        int segments = 20);

    std::shared_ptr<Model> CreateSphere(float radius = 0.5f, 
        int slices = 24, 
        int stacks = 16);

    std::shared_ptr<Model> CreateCapsule(float radius = 0.5f, 
        float height = 2.f, 
        int slices = 24, 
        int stacks = 8);

}
