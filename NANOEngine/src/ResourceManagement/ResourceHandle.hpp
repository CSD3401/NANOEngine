#ifndef RESOURCE_HANDLE_HPP
#define RESOURCE_HANDLE_HPP

#include <cstdint>
#include <memory>

namespace NE::Resource {
	using ResourceHandle = uint64_t;
	constexpr ResourceHandle InvalidResourceHandle = 0;

	struct IResource {};

	struct ResourceHandle {
		std::shared_ptr<IResource> resource;
		uint32_t refCount;
	};
}


#endif // !RESOURCE_HANDLE_HPP
