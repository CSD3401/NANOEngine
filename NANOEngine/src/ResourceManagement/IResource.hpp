#ifndef IRESOURCE_HPP
#define IRESOURCE_HPP

#include "BinaryView.hpp"
#include "NANOEngineAPI.hpp"

namespace NE::Resource {

	struct NANOENGINE_API IResource {
		virtual ~IResource() = default;

		virtual bool Preload(BinaryView blob) = 0;
		virtual void Finalize() = 0;
	};

}

#endif // !IRESOURCE_HPP
