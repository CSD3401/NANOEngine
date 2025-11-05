#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include "IResource.hpp"

#include "ResourcePaths.hpp"
#include "Core/SpdLogger.hpp"

namespace NE::Resource {

	class ResourceManager {
	public:
		static ResourceManager& GetInstance();

		template <typename T>
		requires std::derived_from<T, IResource>
		std::shared_ptr<T> LoadResource(const std::string& uuid) {
			{
				std::scoped_lock l(mtx);
				if (auto it = cache.find(uuid); it != cache.end())
					return std::static_pointer_cast<T>(it->second);
			}

			const auto path = ComputeArtifactPathFromUUID(uuid);
			std::vector<uint8_t> bytes;
			if (!ReadBinFile(path, bytes) || bytes.empty()) {
				SPD_WARNING("Failed to read binary: " << uuid);
				return nullptr;
			}

			auto resource = std::make_shared<T>();
			BinaryView view{ bytes.data(), bytes.size() };
			if (!resource->Preload(view)) {
				SPD_WARNING("Failed to load: " << uuid);
				return nullptr;
			}
			resource->Finalize();

			{
				std::scoped_lock l(mtx);
				cache.emplace(uuid, resource);
			}

			return resource;
		}

	private:
		ResourceManager() = default;
		~ResourceManager() = default;

		bool ReadBinFile(const std::string& path, std::vector<uint8_t>& out) const;

		std::unordered_map<std::string, std::shared_ptr<IResource>> cache;
		std::mutex mtx;
	};

}

