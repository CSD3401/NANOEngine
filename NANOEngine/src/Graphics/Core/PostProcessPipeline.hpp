#pragma once

#include <array>
#include <memory>
#include <unordered_map>

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
			const Math::Mat4& currView,
			const Math::Mat4& currProj,
			bool isSceneView);

		void SetSettings(PostProcessingSettings* settings);

		RenderGraph* GetRenderGraph() const { return m_graph.get(); }
		TexturePool* GetTexturePool() const { return m_pool.get(); }

	private:
		void InitFullscreenQuad();
		void InitBloomResources(uint32_t w, uint32_t h);
		void InitSSAOResources(uint32_t w, uint32_t h);
		void EnsureSSRSceneMipResources(uint32_t sourceTex, uint32_t w, uint32_t h);
		void EnsureSSRHiZResources(uint32_t w, uint32_t h);
		void EnsureSSRTemporalResources(RenderViewHandle viewHandle, uint32_t w, uint32_t h, uint32_t sourceDepthTex, uint32_t sourceNormalTex, uint32_t sourceRoughnessTex);
		void EnsureTAAResources(RenderViewHandle viewHandle, uint32_t w, uint32_t h);
		void LoadShaders();
		void SetupGraph(RenderViewHandle sourceView,
			RenderViewHandle destView,
			const Math::Mat4& invProj,
			const Math::Mat4& currView,
			const Math::Mat4& currProj,
			bool isSceneView);
		void DestroyResources(bool destroyQuad);

	private:
		struct PostProcessContext {
			std::shared_ptr<IFrameBuffer> sourceFB;
			std::shared_ptr<IFrameBuffer> destFB;
			Math::Mat4 invProj;
			Math::Mat4 currViewProjInv;
			Math::Mat4 prevViewProj;
			uint32_t taaHistoryTex = 0;
			bool taaHasHistory = false;
			bool isSceneView = true;
		};

		struct TAAViewState {
			std::array<unsigned int, 2> historyTex{};
			std::array<unsigned int, 2> historyFBO{};
			uint32_t width = 0;
			uint32_t height = 0;
			int readIndex = 0;
			int writeIndex = 1;
			bool hasHistory = false;
			Math::Mat4 prevViewProj;
		};
		struct SSRTemporalViewState {
			std::array<unsigned int, 2> historyColorTex{};
			std::array<unsigned int, 2> historyDepthTex{};
			std::array<unsigned int, 2> historyNormalTex{};
			std::array<unsigned int, 2> historyRoughnessTex{};
			uint32_t width = 0;
			uint32_t height = 0;
			int readIndex = 0;
			int writeIndex = 1;
			bool hasHistory = false;
			Math::Mat4 prevViewProj;
			Math::Mat4 prevProj;
			int depthInternalFormat = 0;
			int normalInternalFormat = 0;
			int roughnessInternalFormat = 0;
		};

		static constexpr int BLOOM_LEVELS = 5;

		RenderViewManager* m_rvm = nullptr;
		PostProcessingSettings* m_settings = nullptr;

		std::unique_ptr<RenderGraph> m_graph;
		std::unique_ptr<TexturePool> m_pool;

		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		unsigned int m_QuadVAO = 0;
		unsigned int m_QuadVBO = 0;

		std::shared_ptr<OpenGL::GLShader> m_brightPassShader;
		unsigned int m_brightPassTex = 0;
		unsigned int m_brightPassFBO = 0;

		std::array<unsigned int, BLOOM_LEVELS> m_bloomFBO{};
		std::array<unsigned int, BLOOM_LEVELS> m_bloomTex{};
		std::array<int, BLOOM_LEVELS> m_bloomWidth{};
		std::array<int, BLOOM_LEVELS> m_bloomHeight{};

		std::array<unsigned int, BLOOM_LEVELS> m_bloomTempFBO{};
		std::array<unsigned int, BLOOM_LEVELS> m_bloomTempTex{};

		std::shared_ptr<OpenGL::GLShader> m_downSampleShader;
		std::shared_ptr<OpenGL::GLShader> m_blurShader;
		std::shared_ptr<OpenGL::GLShader> m_upSampleShader;
		std::shared_ptr<OpenGL::GLShader> m_compositeShader;
		std::shared_ptr<OpenGL::GLShader> m_taaShader;
		std::shared_ptr<OpenGL::GLShader> m_ssrShader;
		std::shared_ptr<OpenGL::GLShader> m_ssrResolveShader;
		std::shared_ptr<OpenGL::GLShader> m_ssrHiZBuildShader;
		std::shared_ptr<OpenGL::GLShader> m_ssrTemporalShader;
		std::shared_ptr<OpenGL::GLShader> m_ssrApplyShader;

		unsigned int m_SSAOFBO = 0;
		unsigned int m_SSAOTex = 0;
		std::shared_ptr<OpenGL::GLShader> m_SSAOShader;
		unsigned int m_ssrSceneMipTex = 0;
		unsigned int m_ssrSceneMipFBO = 0;
		uint32_t m_ssrSceneMipWidth = 0;
		uint32_t m_ssrSceneMipHeight = 0;
		int m_ssrSceneMipLevels = 1;
		int m_ssrSceneMipInternalFormat = 0;

		unsigned int m_ssrHiZTex = 0;
		uint32_t m_ssrHiZWidth = 0;
		uint32_t m_ssrHiZHeight = 0;
		int m_ssrHiZLevels = 1;

		std::unordered_map<RenderViewHandle, TAAViewState> m_taaStates;
		std::unordered_map<RenderViewHandle, SSRTemporalViewState> m_ssrTemporalStates;
		uint32_t m_frameIndex = 0;

		PostProcessContext m_context;
	};
}
