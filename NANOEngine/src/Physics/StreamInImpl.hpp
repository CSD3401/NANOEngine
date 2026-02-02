#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/StreamIn.h>

namespace NE::Physics {
	class StreamInImpl final : public JPH::StreamIn {
	public:
		StreamInImpl(const uint8_t* data, size_t size);

		void ReadBytes(void* outData, size_t inNumBytes) override;

		bool IsEOF() const override;
		bool IsFailed() const override;

	private:
		const uint8_t* mData = nullptr;
		size_t mSize = 0;
		size_t mPos = 0;
		bool mFailed = false;
	};
}