#pragma once

#include "Graphics/Core/Material.hpp"
#include "Graphics/Core/Model.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct PERenderer
    {
        enum class RenderMode : uint8_t
        {
            Billboard,          // camera-facing quad
            StretchedBillboard, // align to velocity
            // Mesh             // optional later: render a mesh per particle
        };

        enum class BlendMode : uint8_t
        {
            Alpha,
            Additive,
            Premultiplied
        };

        // =====================================================
        // Exposed (editor/asset pipeline)
        // =====================================================
        std::string materialUUID;      // particle material (uses texture(s) inside)
        std::string modelUUID;         // OPTIONAL: if you want a custom quad model; can be empty and use engine quad
        //RenderMode renderMode = RenderMode::Billboard;
        //BlendMode blendMode = BlendMode::Alpha;

        //bool sortBackToFront = false;  // only for alpha usually
        //bool castShadows = false;      // typically false for particles
        //bool receiveShadows = false;

        // Sprite/flipbook (optional but very common)
        //bool useFlipbook = false;
        //uint16_t tilesX = 1;
        //uint16_t tilesY = 1;

        // =====================================================
        // Internal (resolved runtime pointers)
        // =====================================================
        std::shared_ptr<Graphics::Material> material;
        std::shared_ptr<Graphics::Model> model; // if null, use engine’s built-in quad

        bool isDirty = false;
        uint64_t luid = 0;

        NE_REFLECT_BEGIN(PERenderer)
            NE_REFLECT_FIELD(materialUUID),
            NE_REFLECT_FIELD(modelUUID)
            //NE_REFLECT_FIELD(renderMode),
            //NE_REFLECT_FIELD(blendMode),
            //NE_REFLECT_FIELD(sortBackToFront),
            //NE_REFLECT_FIELD(useFlipbook),
            //NE_REFLECT_FIELD(tilesX),
            //NE_REFLECT_FIELD(tilesY),
            //NE_REFLECT_FIELD_HIDDEN(castShadows),
            //NE_REFLECT_FIELD_HIDDEN(receiveShadows),
        NE_REFLECT_END()
    };

}