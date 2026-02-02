#pragma once
#include <cstdint>
#include <algorithm>

namespace NE::Physics {
    enum class ContactEventType : uint8_t { Added, Persisted, Removed };

    struct RawContactEvent {
        uint64_t aLuid = 0;
        uint64_t bLuid = 0;
        bool     isTrigger = false;
        ContactEventType type;
        // (Optional: manifold info like normal/penetration/impulse if you want)
    };

    struct ContactKey {
        uint64_t a = 0;
        uint64_t b = 0;
        bool     isTrigger = false;

        static ContactKey Make(uint64_t x, uint64_t y, bool trigger)
        {
            ContactKey k;
            k.a = std::min(x, y);
            k.b = std::max(x, y);
            k.isTrigger = trigger;
            return k;
        }

        bool operator==(const ContactKey& o) const
        {
            return a == o.a && b == o.b && isTrigger == o.isTrigger;
        }
    };

    struct ContactKeyHash {
        size_t operator()(const ContactKey& k) const noexcept
        {
            // simple hash combine
            size_t h = std::hash<uint64_t>{}(k.a);
            h ^= (std::hash<uint64_t>{}(k.b) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
            h ^= (std::hash<uint8_t>{}(uint8_t(k.isTrigger)) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
            return h;
        }
    };

}
