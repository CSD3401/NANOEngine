#include "StreamInImpl.hpp"

namespace NE::Physics {
	StreamInImpl::StreamInImpl(const uint8_t* data, size_t size)
		: mData(data), mSize(size) {
	}

	void StreamInImpl::ReadBytes(void* outData, size_t inNumBytes) {
		if (mPos + inNumBytes > mSize) {
			mFailed = true;
			return;
		}
		std::memcpy(outData, mData + mPos, inNumBytes);
		mPos += inNumBytes;
	}

	bool StreamInImpl::IsEOF() const {
		return mPos >= mSize;
	}
	bool StreamInImpl::IsFailed() const {
		return mFailed;
	}
}