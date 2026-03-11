#pragma once

#include <unordered_map>
#include <vector>
#include <memory>

#include "LightShadowRuntime.hpp"

namespace NE {
	namespace Graphics {
		struct RenderView;
		struct DrawCommand;

		namespace OpenGL {
			class GLShader;
		}
	}
	namespace Math {
		struct Mat4;
	}
}

namespace NE::Graphics {
	class ShadowRenderer {
	public:
		void Init();
		void Update(const RenderView& view,
			const std::vector<RenderLightRef>& lights,
			const std::vector<DrawCommand>& commands);
		LightShadowRuntime* FindRuntime(ECS::Entity entity);
		const LightShadowRuntime* FindRuntime(ECS::Entity entity) const;

		void Shutdown();

	private:
		LightShadowRuntime& GetOrCreateRuntime(ECS::Entity entity);
		void PruneUnusedRuntimes(const std::vector<RenderLightRef>& lights);
		void ReleaseRuntimeResources(LightShadowRuntime& runtime);
		void EnsureResources2D(const ECS::Component::Light& light, LightShadowRuntime& runtime);
		Math::Mat4 BuildLightVP(const ECS::Component::Light& light);
		void RenderDepth(const LightShadowRuntime& runtime,
			const Math::Mat4& lightVP,
			const std::vector<DrawCommand>& commands);

		void RenderDepthDirectionalCascade(const LightShadowRuntime& runtime, int cascadeIdx, const NE::Math::Mat4& lightVP, const std::vector<DrawCommand>& commands);

		void ComputeDirectionalSplits(const RenderView& view, LightShadowRuntime& runtime);

		NE::Math::Mat4 BuildDirectionalCascadeVP(const RenderView& view, const ECS::Component::Light& light, const LightShadowRuntime& runtime, int cascadeIdx);

		std::shared_ptr<OpenGL::GLShader> m_shadowShader;
		std::unordered_map<ECS::Entity, LightShadowRuntime> m_lightShadowRuntime;
		int m_shadowRes = 2048;
		float m_directionalShadowDistance = 60.0f;
	};
}
