#pragma once
#include <cstdint>

struct ContactPair {
    uint32_t a;
    uint32_t b;

    bool operator==(const ContactPair& rhs) const
    {
        return (a == rhs.a && b == rhs.b) ||
            (a == rhs.b && b == rhs.a);
    }
};
