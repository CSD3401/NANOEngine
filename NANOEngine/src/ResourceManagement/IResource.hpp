#ifndef IRESOURCE_HPP
#define IRESOURCE_HPP

#include "BinaryView.hpp"

namespace NE::Resource {

	struct IResource {
		virtual ~IResource() = default;

		virtual bool Preload(BinaryView blob) = 0;
		virtual void Finalize() {}
	};

}

#endif // !IRESOURCE_HPP
