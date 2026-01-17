#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include "../NANOEngineAPI.hpp"

// Forward Decl
namespace NE::ECS::Component {
	struct Renderer;
}

namespace NE::Graphics {
	struct RenderSettings;
	struct PostProcessingSettings;
	class Material;
	class RenderGraph;
	class TexturePool;
}

namespace NE::Renderer {

	namespace Query {
		NANOENGINE_API std::string GetModel(uint32_t e);
		NANOENGINE_API std::string GetMaterial(uint32_t e);
		NANOENGINE_API const Graphics::RenderSettings& GetRenderSettings();
		NANOENGINE_API const Graphics::PostProcessingSettings& GetPostProcessingSettings();
		NANOENGINE_API Graphics::RenderGraph* GetRenderGraph();
		NANOENGINE_API Graphics::TexturePool* GetTexturePool();
	}

	namespace Command {
		NANOENGINE_API std::shared_ptr<NE::Graphics::Material> GetMaterial(const std::string& uuid);
		NANOENGINE_API void AssignModel(uint32_t e, const std::string& uuid);
		NANOENGINE_API void AssignMaterial(uint32_t e, const std::string& uuid);
		NANOENGINE_API void AssignUITexture(uint32_t e, const std::string& textureUUID, const std::string& materialUUID);
		NANOENGINE_API Graphics::RenderSettings& GetRenderSettings();
		NANOENGINE_API Graphics::PostProcessingSettings& GetPostProcessingSettings();
	}
}
