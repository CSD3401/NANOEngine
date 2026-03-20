#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include "../NANOEngineAPI.hpp"
#include "../Math/Vec3.hpp"

// Forward Decl
namespace NE::ECS::Component {
	struct Renderer;
	struct Light;
	struct DecalProjector;
	struct Transform;
}

namespace NE::Graphics {
	struct RenderSettings;
	struct PostProcessingSettings;
	struct SelectionHighlightSettings;
	class Material;
	class RenderGraph;
	class TexturePool;
	class Model;
}

namespace NE::Renderer {

	namespace Query {
		NANOENGINE_API std::string GetModel(uint32_t e);
		NANOENGINE_API std::string GetMaterial(uint32_t e);
		NANOENGINE_API const Graphics::RenderSettings& GetRenderSettings();
		NANOENGINE_API const Graphics::PostProcessingSettings& GetPostProcessingSettings();
		NANOENGINE_API const Graphics::SelectionHighlightSettings& GetSelectionHighlightSettings();
		NANOENGINE_API Graphics::RenderGraph* GetRenderGraph();
		NANOENGINE_API Graphics::TexturePool* GetTexturePool();
	}

	namespace Command {
		NANOENGINE_API std::shared_ptr<NE::Graphics::Material> GetMaterial(const std::string& uuid);
		NANOENGINE_API std::shared_ptr<NE::Graphics::Model> GetModel(const std::string& uuid);
		NANOENGINE_API void AssignModel(uint32_t e, const std::string& uuid);
		NANOENGINE_API void AssignModel(uint32_t e, const std::string& uuid, int32_t submeshIndex);
		NANOENGINE_API void AssignMaterial(uint32_t e, const std::string& uuid);
		NANOENGINE_API void AssignUITexture(uint32_t e, const std::string& textureUUID, const std::string& materialUUID);
		NANOENGINE_API Graphics::RenderSettings& GetRenderSettings();
		NANOENGINE_API Graphics::PostProcessingSettings& GetPostProcessingSettings();
		NANOENGINE_API Graphics::SelectionHighlightSettings& GetSelectionHighlightSettings();
		NANOENGINE_API void DrawSelectedLightGizmos(const NE::ECS::Component::Light& lightComponent);
		NANOENGINE_API void DrawSelectedDecalGizmos(const NE::ECS::Component::DecalProjector& decalComponent, 
			const NE::ECS::Component::Transform& transformComponent);
		NANOENGINE_API void AddDebugLinesBatch(const std::vector<NE::Math::Vec3>& positions, const NE::Math::Vec3& color);
		NANOENGINE_API void SetSelectedEntities(const std::vector<uint32_t>& selectedIds);
		NANOENGINE_API void ClearSelectedEntities();
	}
}
