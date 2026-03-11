#pragma once

#include <vector>
#include <memory>

namespace NE {
	namespace ECS {
		namespace Component {
			struct Light;
		}
	}
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
			std::vector<ECS::Component::Light*>& lights,
			const std::vector<DrawCommand>& commands);

		void Shutdown();

	private:
		void EnsureResources2D(ECS::Component::Light& light);
		Math::Mat4 BuildLightVP(const ECS::Component::Light& light);
		void RenderDepth(ECS::Component::Light& light,
			const Math::Mat4& lightVP,
			const std::vector<DrawCommand>& commands);

		void RenderDepthDirectionalCascade(ECS::Component::Light& light, int cascadeIdx, const NE::Math::Mat4& lightVP, const std::vector<DrawCommand>& commands);

		void ComputeDirectionalSplits(const RenderView& view, ECS::Component::Light& light);

		NE::Math::Mat4 BuildDirectionalCascadeVP(const RenderView& view, const ECS::Component::Light& light, int cascadeIdx);

		std::shared_ptr<OpenGL::GLShader> m_shadowShader;
		int m_shadowRes = 2048;
		float m_directionalShadowDistance = 60.0f;
	};
}
