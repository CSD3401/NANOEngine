#include "pch.h"
#include "StreamOutImpl.hpp"

namespace NE::Physics {
    StreamOutImpl::StreamOutImpl(std::vector<uint8_t>& out) 
        : mOut(out) {}

    void StreamOutImpl::WriteBytes(const void* inData, size_t inNumBytes) {
        size_t old = mOut.size();
        mOut.resize(old + inNumBytes);
        std::memcpy(mOut.data() + old, inData, inNumBytes);
    }

    bool StreamOutImpl::IsFailed() const { 
        return false; 
    }
}
