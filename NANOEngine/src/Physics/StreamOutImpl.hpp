#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/StreamOut.h>

namespace NE::Physics {
    class StreamOutImpl final : public JPH::StreamOut {
    public:
        StreamOutImpl(std::vector<uint8_t>& out);
        void WriteBytes(const void* inData, size_t inNumBytes) override;
        bool IsFailed() const override;

    private:
        std::vector<uint8_t>& mOut;
    };
}

