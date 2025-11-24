#include "RendererExports.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Components/Renderer.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "../EngineState.hpp"  // For GetEngineState
#include "../Engine.hpp"  // For MarkSceneDirty
#include <Core/SpdLogger.hpp>
#include "../../include/ScriptSDK/ScriptTypes.h"
#include <Graphics/Core/GraphicsManager.hpp>

namespace NE {
	SceneManagement::Scene& GetScene();

	namespace Scripting {
		// Forward declaration of helper function from ScriptAPI.cpp
		std::string GetMaterialUUIDFromRef(const MaterialRef& materialRef);
	}
}

namespace NE::Renderer {

	namespace Query {
		NANOENGINE_API std::string GetModel(uint32_t e)
		{
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			if (r.modelUUID.empty()) return "empty uuid";
			else return r.modelUUID;
		}
		NANOENGINE_API std::string GetMaterial(uint32_t e)
		{
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			if (r.materialUUID.empty()) return "empty uuid";
			else return r.materialUUID;
		}

		NANOENGINE_API std::string GetMaterialUUID(const NE::Scripting::MaterialRef& materialRef)
		{
			return NE::Scripting::GetMaterialUUIDFromRef(materialRef);
		}
		//FOG
		bool  GetFogEnabled() { return NE::Graphics::GraphicsManager::GetFog().enabled; }
		int   GetFogMode() { return (int)NE::Graphics::GraphicsManager::GetFog().mode; }
		void  GetFogColor(float c[3]) { auto f = NE::Graphics::GraphicsManager::GetFog(); c[0] = f.color.x; c[1] = f.color.y; c[2] = f.color.z; }
		float GetFogDensity() { return NE::Graphics::GraphicsManager::GetFog().density; }
		void  GetFogRange(float& s, float& e) { auto f = NE::Graphics::GraphicsManager::GetFog(); s = f.start; e = f.end; }
	//END FOG
	}

	namespace Command {
		void AssignModel(uint32_t e, const std::string& uuid) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			r.modelUUID = uuid;
			r.model = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Model>(uuid);

			if (r.materialUUID.empty()) {
				r.materialUUID = "neunlitmat";
				r.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>("neunlitmat");
			}
			
			// Mark component and scene dirty (Edit mode only)
			if (NE::GetEngineState() == NE::EngineState::Edit) {
				if constexpr (requires { r.isDirty; }) r.isDirty = true;
				NE::MarkSceneDirty();
				SPD_DEBUG("[DirtyFlag] Model changed - Scene marked DIRTY");
			}
		}

		void AssignMaterial(uint32_t e, const std::string& uuid) {
			auto& r = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Renderer>(e);
			r.materialUUID = uuid;
			r.material = Resource::ResourceManager::GetInstance().LoadResource<Graphics::Material>(uuid);
			
			// Mark component and scene dirty (Edit mode only)
			if (NE::GetEngineState() == NE::EngineState::Edit) {
				if constexpr (requires { r.isDirty; }) r.isDirty = true;
				NE::MarkSceneDirty();
				SPD_DEBUG("[DirtyFlag] Material changed - Scene marked DIRTY");
			}
		}
//FOG
		void SetFogEnabled(bool v) { auto f = NE::Graphics::GraphicsManager::GetFog(); f.enabled = v; NE::Graphics::GraphicsManager::SetFog(f); }
		void SetFogMode(int m) { auto f = NE::Graphics::GraphicsManager::GetFog(); f.mode = (NE::Graphics::GraphicsManager::FogMode)m; NE::Graphics::GraphicsManager::SetFog(f); }
		void SetFogColor(float r, float g, float b) { auto f = NE::Graphics::GraphicsManager::GetFog(); f.color = { r,g,b }; NE::Graphics::GraphicsManager::SetFog(f); }
		void SetFogDensity(float d) { auto f = NE::Graphics::GraphicsManager::GetFog(); f.density = d; NE::Graphics::GraphicsManager::SetFog(f); }
		void SetFogRange(float s, float e) { auto f = NE::Graphics::GraphicsManager::GetFog(); f.start = s; f.end = e; NE::Graphics::GraphicsManager::SetFog(f); }
	//END FOG
	}

}
