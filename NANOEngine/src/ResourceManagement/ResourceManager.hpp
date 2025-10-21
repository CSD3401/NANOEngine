#pragma once

namespace NE::Resource {

	class ResourceManager {
	public:
		static ResourceManager& GetInstance();




	private:
		ResourceManager() = default;
		~ResourceManager() = default;
	};

}

