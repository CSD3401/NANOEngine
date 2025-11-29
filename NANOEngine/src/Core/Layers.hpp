#pragma once
#include <cstdint>
#include <initializer_list>

namespace NE::Core::Layers {
    using Layer = uint8_t;
    using Mask = uint32_t;

    enum : Layer {
        Default = 0,
        Environment = 1,
        Player = 2,
        Enemy = 3,
        Layer4 = 4,
        Layer5 = 5,
        Layer6 = 6,
        Layer7 = 7,
        Layer8 = 8,
        Layer9 = 9,
        Layer10 = 10,
        Layer11 = 11,
        Layer12 = 12,
        Layer13 = 13
    };

    inline Mask ToMask(Layer layer) { return 1u << layer; }
    inline Mask AnyOf(std::initializer_list<Layer> layers)
    {
        Mask m = 0;
        for (Layer l : layers) m |= (1u << l);
        return m;
    }

    inline bool Contains(Mask mask, Layer layer)
    {
        return (mask & (1u << layer)) != 0;
    }
}

