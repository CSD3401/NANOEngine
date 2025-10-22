#ifndef BINARY_VIEW_HPP
#define BINARY_VIEW_HPP

#include <cstdint>

namespace NE::Resource {

	struct BinaryView {
		const uint8_t* data = nullptr;
		size_t size = 0;

        template<class T>
        const T* as(size_t off = 0) const {
            return (off + sizeof(T) <= size) ? reinterpret_cast<const T*>(data + off) : nullptr;
        }
        const uint8_t* at(size_t off, size_t count) const {
            return (off + count <= size) ? data + off : nullptr;
        }
	};

}

#endif // !BINARY_VIEW_HPP
