#pragma once

#include <array>
#include <memory>

#include "RenderGraph/TexturePool.hpp"
#include "RenderGraph.hpp"
#include "RenderViewManager.hpp"

namespace NE {
	namespace Math {
		struct Mat4;
	}
	namespace Graphics {
		namespace OpenGL {
			class GLShader;
		}
		//class RenderGraph;
		//class TexturePool;
		class IFrameBuffer;
		class RenderViewManager;
		struct PostProcessingSettings;
	}
}

namespace NE::Graphics {
	class PostProcessPipeline {
	public:
		void Init(RenderViewManager* rvm, uint32_t initialW, uint32_t initialH);
		void Shutdown();
		void Resize(uint32_t w, uint32_t h);

		void Execute(RenderViewHandle sourceView,
			RenderViewHandle destView,
			const Math::Mat4& invProj,
			bool isSceneView);

		void SetSettings(PostProcessingSettings* settings);

		RenderGraph* GetRenderGraph() const { return m_Graph.get(); }
		TexturePool* GetTexturePool() const { return m_Pool.get(); }

	private:
		void InitFullscreenQuad();
		void InitBloomResources(uint32_t w, uint32_t h);
		void InitSSAOResources(uint32_t w, uint32_t h);
		void LoadShaders();
		void SetupGraph(RenderViewHandle sourceView,
			RenderViewHandle destView,
			const Math::Mat4& invProj,
			bool isSceneView);
		void DestroyResources(bool destroyQuad);

	private:
		struct PostProcessContext {
			std::shared_ptr<IFrameBuffer> sourceFB;
			std::shared_ptr<IFrameBuffer> destFB;
			Math::Mat4 invProj;
			bool isSceneView = true;
		};

		static constexpr int BLOOM_LEVELS = 5;

		RenderViewManager* m_RVM = nullptr;
		PostProcessingSettings* m_Settings = nullptr;

		std::unique_ptr<RenderGraph> m_Graph;
		std::unique_ptr<TexturePool> m_Pool;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		unsigned int m_QuadVAO = 0;
		unsigned int m_QuadVBO = 0;

		std::shared_ptr<OpenGL::GLShader> m_BrightPassShader;
		unsigned int m_BrightPassTex = 0;
		unsigned int m_BrightPassFBO = 0;

		std::array<unsigned int, BLOOM_LEVELS> m_BloomFBO{};
		std::array<unsigned int, BLOOM_LEVELS> m_BloomTex{};
		std::array<int, BLOOM_LEVELS> m_BloomWidth{};
		std::array<int, BLOOM_LEVELS> m_BloomHeight{};

		std::array<unsigned int, BLOOM_LEVELS> m_BloomTempFBO{};
		std::array<unsigned int, BLOOM_LEVELS> m_BloomTempTex{};

		std::shared_ptr<OpenGL::GLShader> m_DownSampleShader;
		std::shared_ptr<OpenGL::GLShader> m_BlurShader;
		std::shared_ptr<OpenGL::GLShader> m_UpSampleShader;
		std::shared_ptr<OpenGL::GLShader> m_CompositeShader;

		unsigned int m_SSAOFBO = 0;
		unsigned int m_SSAOTex = 0;
		std::shared_ptr<OpenGL::GLShader> m_SSAOShader;

		PostProcessContext m_Context;
	};
}
