#ifndef IRESOURCE_HPP
#define IRESOURCE_HPP

#include "BinaryView.hpp"

namespace NE::Resource {

	struct IResource {
		virtual ~IResource() = default;
		// Parse cooked blob (no file I/O). Do NOT touch GL/driver here if you’ll go async later.
		virtual bool Preload(BinaryView blob) = 0;
		// Create/upload GPU resources (call on render thread). For now you can do real work here.
		virtual void Finalize() {}
	};

}

#endif // !IRESOURCE_HPP
