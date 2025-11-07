#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

#include "IResource.hpp"
#include "ResourcePaths.hpp"
#include "Core/SpdLogger.hpp"
#include "Graphics/Core/Primitives.hpp"
//#include "Graphics/OpenGL/GLShader.hpp"

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

			if (auto builtin = TryCreateBuiltin<T>(uuid)) {
				std::scoped_lock l(mtx);
				cache.emplace(uuid, builtin);
				return builtin;
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

			if constexpr (requires (T t) { t.uuid; }) {
				resource->uuid = uuid;
			}

			{
				std::scoped_lock l(mtx);
				cache.emplace(uuid, resource);
			}

			return resource;
		}

		template<typename T>
		std::shared_ptr<T> TryCreateBuiltin(const std::string& id) {
			if constexpr (std::is_same_v<T, NE::Graphics::Model>) {
				if (id == "builtin:model/cube")
					return std::static_pointer_cast<T>(NE::Graphics::CreateCube());
				if (id == "builtin:model/plane")
					return std::static_pointer_cast<T>(NE::Graphics::CreatePlane());
				if (id == "builtin:model/cylinder")
					return std::static_pointer_cast<T>(NE::Graphics::CreateCylinder());
				if (id == "builtin:model/sphere")
					return std::static_pointer_cast<T>(NE::Graphics::CreateSphere());
				if (id == "builtin:model/capsule")
					return std::static_pointer_cast<T>(NE::Graphics::CreateCapsule());
			} 
			//else if constexpr (std::is_same_v<T, NE::Graphics::OpenGL::Shader>)
			return nullptr;
		}

	private:
		ResourceManager() = default;
		~ResourceManager() = default;

		bool ReadBinFile(const std::string& path, std::vector<uint8_t>& out) const;

		std::unordered_map<std::string, std::shared_ptr<IResource>> cache;
		std::mutex mtx;
	};

}

