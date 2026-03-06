#include "pch.h"
#include "PostProcessPipeline.hpp"

#include "PostProcessingSettings.hpp"
#include "Math/Mat4.hpp"
#include "RenderGraph.hpp"
#include "RenderViewManager.hpp"
#include "../Interfaces/IFrameBuffer.hpp"
#include "../OpenGL/GLShader.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "Core/Logger.hpp"
#include "Core/Profiler.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glad/glad.h>
#include <GL/gl.h>

namespace NE::Graphics {
	namespace {
		GLint QueryTextureInternalFormat(uint32_t texture) {
			if (texture == 0) return 0;
			GLint internalFormat = 0;
			glBindTexture(GL_TEXTURE_2D, texture);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
			glBindTexture(GL_TEXTURE_2D, 0);
			return internalFormat;
		}

		void ResolvePixelFormatAndType(GLint internalFormat, GLenum& format, GLenum& type) {
			switch (internalFormat) {
			case GL_RGBA16F: format = GL_RGBA; type = GL_FLOAT; break;
			case GL_RGBA8: format = GL_RGBA; type = GL_UNSIGNED_BYTE; break;
			case GL_R8: format = GL_RED; type = GL_UNSIGNED_BYTE; break;
			case GL_R16F: format = GL_RED; type = GL_FLOAT; break;
			case GL_DEPTH_COMPONENT16: format = GL_DEPTH_COMPONENT; type = GL_UNSIGNED_SHORT; break;
			case GL_DEPTH_COMPONENT24: format = GL_DEPTH_COMPONENT; type = GL_UNSIGNED_INT; break;
			case GL_DEPTH_COMPONENT32F: format = GL_DEPTH_COMPONENT; type = GL_FLOAT; break;
			case GL_DEPTH24_STENCIL8: format = GL_DEPTH_STENCIL; type = GL_UNSIGNED_INT_24_8; break;
			default: format = GL_RGBA; type = GL_FLOAT; break;
			}
		}

		void ClearTexture(uint32_t texture, GLint internalFormat) {
			if (texture == 0) return;
			switch (internalFormat) {
			case GL_DEPTH_COMPONENT16:
			case GL_DEPTH_COMPONENT24:
			case GL_DEPTH_COMPONENT32F: {
				const float depthClear = 1.0f;
				glClearTexImage(texture, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &depthClear);
				break;
			}
			case GL_DEPTH24_STENCIL8: {
				struct DepthStencilClearValue {
					float depth;
					uint32_t stencil;
				} clearValue = { 1.0f, 0u };
				glClearTexImage(texture, 0, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, &clearValue);
				break;
			}
			case GL_R8: {
				const uint8_t zero = 0u;
				glClearTexImage(texture, 0, GL_RED, GL_UNSIGNED_BYTE, &zero);
				break;
			}
			default: {
				const float zeros[4] = { 0.f, 0.f, 0.f, 0.f };
				glClearTexImage(texture, 0, GL_RGBA, GL_FLOAT, zeros);
				break;
			}
			}
		}

		float MaxAbsMatrixDelta(const Math::Mat4& a, const Math::Mat4& b) {
			float maxDelta = 0.0f;
			for (unsigned i = 0; i < 16; ++i) {
				maxDelta = std::max(maxDelta, std::abs(a[i] - b[i]));
			}
			return maxDelta;
		}
	}

	void PostProcessPipeline::Init(RenderViewManager* rvm, uint32_t initialW, uint32_t initialH) {
		m_rvm = rvm;
		m_Width = initialW;
		m_Height = initialH;

		m_pool = std::make_unique<TexturePool>();
		m_graph = std::make_unique<RenderGraph>();
		m_graph->SetTexturePool(m_pool.get());

		InitFullscreenQuad();
		LoadShaders();
		InitBloomResources(m_Width, m_Height);
		InitSSAOResources(m_Width, m_Height);
	}

	void PostProcessPipeline::Shutdown() {
		DestroyResources(true);
		m_graph.reset();
		m_pool.reset();
		m_rvm = nullptr;
	}

	void PostProcessPipeline::Resize(uint32_t w, uint32_t h) {
		if (w == 0 || h == 0) return;
		if (w == m_Width && h == m_Height) return;

		m_Width = w;
		m_Height = h;

		DestroyResources(false);
		InitBloomResources(m_Width, m_Height);
		InitSSAOResources(m_Width, m_Height);
	}

	void PostProcessPipeline::Execute(RenderViewHandle sourceView,
		RenderViewHandle destView,
		const Math::Mat4& invProj,
		const Math::Mat4& currView,
		const Math::Mat4& currProj,
		bool isSceneView
	) {
#ifndef PRODUCTION_BUILD
		NE_PROFILE_FUNCTION();
#endif

		if (!m_rvm || !m_graph) return;

		if (m_settings && m_settings->taaSettings.resetHistory) {
			for (auto& [_, state] : m_taaStates) {
				state.hasHistory = false;
			}
			for (auto& [_, state] : m_ssrTemporalStates) {
				state.hasHistory = false;
			}
			m_settings->taaSettings.resetHistory = false;
		}

		m_context.invProj = invProj;
		m_context.isSceneView = isSceneView;
		m_context.currViewProjInv = (currProj * currView).Inverse();

		SetupGraph(sourceView, destView, invProj, currView, currProj, isSceneView);
		m_graph->Execute();
		++m_frameIndex;
	}

	void PostProcessPipeline::SetSettings(PostProcessingSettings* settings) {
		m_settings = settings;
	}

	void PostProcessPipeline::SetSelectionSettings(SelectionHighlightSettings* settings) {
		m_selectionSettings = settings;
	}

#ifndef PRODUCTION_BUILD
	void PostProcessPipeline::SetSelectedEntityIds(const std::unordered_set<uint32_t>* selectedIds) {
		m_selectedEntityIds = selectedIds;
	}
#endif

	void PostProcessPipeline::InitFullscreenQuad() {
		if (m_QuadVAO != 0) return;

		float quadVerts[] = {
			-1.f, -1.f, 0.f, 0.f,
			 1.f, -1.f, 1.f, 0.f,
			-1.f,  1.f, 0.f, 1.f,
			 1.f,  1.f, 1.f, 1.f,
		};

		glGenVertexArrays(1, &m_QuadVAO);
		glGenBuffers(1, &m_QuadVBO);
		glBindVertexArray(m_QuadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

		glBindVertexArray(0);
	}

	void PostProcessPipeline::InitBloomResources(uint32_t w, uint32_t h) {
		if (w == 0 || h == 0) return;

		constexpr GLint bloomInternalFormat = GL_RGB16F;
		constexpr GLenum bloomUploadFormat = GL_RGB;
		constexpr GLenum bloomUploadType = GL_FLOAT;

		GLenum att = GL_COLOR_ATTACHMENT0;
		int levelW = std::max(1, static_cast<int>(w / BLOOM_BASE_DIVISOR));
		int levelH = std::max(1, static_cast<int>(h / BLOOM_BASE_DIVISOR));

		for (int i = 0; i < BLOOM_LEVELS; ++i) {
			m_bloomWidth[i] = levelW;
			m_bloomHeight[i] = levelH;

			if (m_bloomFBO[i] == 0) {
				glGenFramebuffers(1, &m_bloomFBO[i]);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[i]);

			if (m_bloomTex[i] == 0) {
				glGenTextures(1, &m_bloomTex[i]);
			}
			glBindTexture(GL_TEXTURE_2D, m_bloomTex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, bloomInternalFormat,
				levelW, levelH, 0,
				bloomUploadFormat, bloomUploadType, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, m_bloomTex[i], 0);
			glDrawBuffers(1, &att);
#ifndef PRODUCTION_BUILD
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				LOG_ERROR("Bloom FBO incomplete! level=" << i);
			}
#endif

			if (m_bloomTempFBO[i] == 0) {
				glGenFramebuffers(1, &m_bloomTempFBO[i]);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, m_bloomTempFBO[i]);

			if (m_bloomTempTex[i] == 0) {
				glGenTextures(1, &m_bloomTempTex[i]);
			}
			glBindTexture(GL_TEXTURE_2D, m_bloomTempTex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, bloomInternalFormat,
				levelW, levelH, 0,
				bloomUploadFormat, bloomUploadType, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, m_bloomTempTex[i], 0);

			glDrawBuffers(1, &att);
#ifndef PRODUCTION_BUILD
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				LOG_ERROR("Bloom temp FBO incomplete! level=" << i);
			}
#endif

			levelW = std::max(1, levelW / 2);
			levelH = std::max(1, levelH / 2);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void PostProcessPipeline::InitSSAOResources(uint32_t w, uint32_t h)
	{
		if (w == 0 || h == 0) return;

		if (m_SSAOFBO == 0) {
			glGenFramebuffers(1, &m_SSAOFBO);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, m_SSAOFBO);

		if (m_SSAOTex == 0) {
			glGenTextures(1, &m_SSAOTex);
		}
		glBindTexture(GL_TEXTURE_2D, m_SSAOTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
			static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
			GL_RED, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, m_SSAOTex, 0);
		GLenum att = GL_COLOR_ATTACHMENT0;
		glDrawBuffers(1, &att);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("SSAO FBO incomplete!");
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void PostProcessPipeline::EnsureSSRSceneMipResources(uint32_t sourceTex, uint32_t w, uint32_t h)
	{
		if (w == 0 || h == 0) return;
		GLint sourceInternalFormat = GL_RGBA16F;
		if (sourceTex != 0) {
			glBindTexture(GL_TEXTURE_2D, sourceTex);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &sourceInternalFormat);
			glBindTexture(GL_TEXTURE_2D, 0);
			if (sourceInternalFormat == 0) {
				sourceInternalFormat = GL_RGBA16F;
			}
		}

		if (m_ssrSceneMipTex != 0
			&& m_ssrSceneMipWidth == w
			&& m_ssrSceneMipHeight == h
			&& m_ssrSceneMipInternalFormat == sourceInternalFormat) {
			return;
		}

		if (m_ssrSceneMipTex != 0) {
			glDeleteTextures(1, &m_ssrSceneMipTex);
			m_ssrSceneMipTex = 0;
		}
		if (m_ssrSceneMipFBO != 0) {
			glDeleteFramebuffers(1, &m_ssrSceneMipFBO);
			m_ssrSceneMipFBO = 0;
		}

		m_ssrSceneMipWidth = w;
		m_ssrSceneMipHeight = h;
		m_ssrSceneMipLevels = static_cast<int>(std::floor(std::log2(static_cast<float>(std::max(w, h))))) + 1;
		m_ssrSceneMipLevels = std::max(1, m_ssrSceneMipLevels);
		m_ssrSceneMipInternalFormat = sourceInternalFormat;

		glGenTextures(1, &m_ssrSceneMipTex);
		glBindTexture(GL_TEXTURE_2D, m_ssrSceneMipTex);
		glTexStorage2D(GL_TEXTURE_2D, m_ssrSceneMipLevels, m_ssrSceneMipInternalFormat, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m_ssrSceneMipLevels - 1);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenFramebuffers(1, &m_ssrSceneMipFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, m_ssrSceneMipFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssrSceneMipTex, 0);
		const GLenum att = GL_COLOR_ATTACHMENT0;
		glDrawBuffers(1, &att);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_ERROR("SSR mip source FBO incomplete!");
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void PostProcessPipeline::EnsureSSRHiZResources(uint32_t w, uint32_t h)
	{
		if (w == 0 || h == 0) return;

		const int levels = std::max(1, static_cast<int>(std::floor(std::log2(static_cast<float>(std::max(w, h))))) + 1);
		if (m_ssrHiZTex != 0
			&& m_ssrHiZWidth == w
			&& m_ssrHiZHeight == h
			&& m_ssrHiZLevels == levels) {
			return;
		}

		if (m_ssrHiZTex != 0) {
			glDeleteTextures(1, &m_ssrHiZTex);
			m_ssrHiZTex = 0;
		}

		m_ssrHiZWidth = w;
		m_ssrHiZHeight = h;
		m_ssrHiZLevels = levels;

		glGenTextures(1, &m_ssrHiZTex);
		glBindTexture(GL_TEXTURE_2D, m_ssrHiZTex);
		glTexStorage2D(GL_TEXTURE_2D, m_ssrHiZLevels, GL_R32F, static_cast<GLsizei>(w), static_cast<GLsizei>(h));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m_ssrHiZLevels - 1);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Initialize to far depth, so MIN-reduction remains conservative.
		const float farDepth = 1.0f;
		for (int mip = 0; mip < m_ssrHiZLevels; ++mip) {
			glClearTexImage(m_ssrHiZTex, mip, GL_RED, GL_FLOAT, &farDepth);
		}
	}

	void PostProcessPipeline::EnsureSSRTemporalResources(RenderViewHandle viewHandle, uint32_t w, uint32_t h, uint32_t sourceDepthTex, uint32_t sourceNormalTex, uint32_t sourceRoughnessTex)
	{
		if (w == 0 || h == 0) return;

		GLint depthFormat = QueryTextureInternalFormat(sourceDepthTex);
		GLint normalFormat = QueryTextureInternalFormat(sourceNormalTex);
		GLint roughnessFormat = QueryTextureInternalFormat(sourceRoughnessTex);
		if (depthFormat == 0) depthFormat = GL_DEPTH_COMPONENT24;
		if (normalFormat == 0) normalFormat = GL_RGBA16F;
		if (roughnessFormat == 0) roughnessFormat = GL_R8;

		auto& state = m_ssrTemporalStates[viewHandle];
		if (state.width == w
			&& state.height == h
			&& state.depthInternalFormat == depthFormat
			&& state.normalInternalFormat == normalFormat
			&& state.roughnessInternalFormat == roughnessFormat
			&& state.historyColorTex[0] != 0 && state.historyColorTex[1] != 0
			&& state.historyDepthTex[0] != 0 && state.historyDepthTex[1] != 0
			&& state.historyNormalTex[0] != 0 && state.historyNormalTex[1] != 0
			&& state.historyRoughnessTex[0] != 0 && state.historyRoughnessTex[1] != 0) {
			return;
		}

		for (int i = 0; i < 2; ++i) {
			if (state.historyColorTex[i] != 0) {
				glDeleteTextures(1, &state.historyColorTex[i]);
				state.historyColorTex[i] = 0;
			}
			if (state.historyDepthTex[i] != 0) {
				glDeleteTextures(1, &state.historyDepthTex[i]);
				state.historyDepthTex[i] = 0;
			}
			if (state.historyNormalTex[i] != 0) {
				glDeleteTextures(1, &state.historyNormalTex[i]);
				state.historyNormalTex[i] = 0;
			}
			if (state.historyRoughnessTex[i] != 0) {
				glDeleteTextures(1, &state.historyRoughnessTex[i]);
				state.historyRoughnessTex[i] = 0;
			}
		}

		state.width = w;
		state.height = h;
		state.readIndex = 0;
		state.writeIndex = 1;
		state.hasHistory = false;
		state.prevViewProj.SetToIdentity();
		state.prevProj.SetToIdentity();
		state.depthInternalFormat = depthFormat;
		state.normalInternalFormat = normalFormat;
		state.roughnessInternalFormat = roughnessFormat;

		for (int i = 0; i < 2; ++i) {
			glGenTextures(1, &state.historyColorTex[i]);
			glBindTexture(GL_TEXTURE_2D, state.historyColorTex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
				static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
				GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			ClearTexture(state.historyColorTex[i], GL_RGBA16F);

			glGenTextures(1, &state.historyDepthTex[i]);
			glBindTexture(GL_TEXTURE_2D, state.historyDepthTex[i]);
			{
				GLenum depthFormatExternal = GL_DEPTH_COMPONENT;
				GLenum depthType = GL_FLOAT;
				ResolvePixelFormatAndType(depthFormat, depthFormatExternal, depthType);
				glTexImage2D(GL_TEXTURE_2D, 0, depthFormat,
					static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
					depthFormatExternal, depthType, nullptr);
			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			ClearTexture(state.historyDepthTex[i], depthFormat);

			glGenTextures(1, &state.historyNormalTex[i]);
			glBindTexture(GL_TEXTURE_2D, state.historyNormalTex[i]);
			{
				GLenum normalFormatExternal = GL_RGBA;
				GLenum normalType = GL_FLOAT;
				ResolvePixelFormatAndType(normalFormat, normalFormatExternal, normalType);
				glTexImage2D(GL_TEXTURE_2D, 0, normalFormat,
					static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
					normalFormatExternal, normalType, nullptr);
			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			ClearTexture(state.historyNormalTex[i], normalFormat);

			glGenTextures(1, &state.historyRoughnessTex[i]);
			glBindTexture(GL_TEXTURE_2D, state.historyRoughnessTex[i]);
			{
				GLenum roughnessFormatExternal = GL_RED;
				GLenum roughnessType = GL_UNSIGNED_BYTE;
				ResolvePixelFormatAndType(roughnessFormat, roughnessFormatExternal, roughnessType);
				glTexImage2D(GL_TEXTURE_2D, 0, roughnessFormat,
					static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
					roughnessFormatExternal, roughnessType, nullptr);
			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			ClearTexture(state.historyRoughnessTex[i], roughnessFormat);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void PostProcessPipeline::EnsureTAAResources(RenderViewHandle viewHandle, uint32_t w, uint32_t h)
	{
		if (w == 0 || h == 0) return;

		auto& state = m_taaStates[viewHandle];
		if (state.width == w && state.height == h && state.historyTex[0] != 0 && state.historyTex[1] != 0) {
			return;
		}

		for (int i = 0; i < 2; ++i) {
			if (state.historyTex[i] != 0) {
				glDeleteTextures(1, &state.historyTex[i]);
				state.historyTex[i] = 0;
			}
			if (state.historyFBO[i] != 0) {
				glDeleteFramebuffers(1, &state.historyFBO[i]);
				state.historyFBO[i] = 0;
			}
		}

		state.width = w;
		state.height = h;
		state.readIndex = 0;
		state.writeIndex = 1;
		state.hasHistory = false;
		state.prevViewProj.SetToIdentity();

		for (int i = 0; i < 2; ++i) {
			glGenTextures(1, &state.historyTex[i]);
			glBindTexture(GL_TEXTURE_2D, state.historyTex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
				static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
				GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glGenFramebuffers(1, &state.historyFBO[i]);
			glBindFramebuffer(GL_FRAMEBUFFER, state.historyFBO[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, state.historyTex[i], 0);
			const GLenum att = GL_COLOR_ATTACHMENT0;
			glDrawBuffers(1, &att);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				LOG_ERROR("TAA history FBO incomplete!");
			}
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void PostProcessPipeline::LoadShaders() {
		if (!m_downSampleShader) {
			m_downSampleShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nebloomdownsample");
		}
		if (!m_upSampleShader) {
			m_upSampleShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nebloomupsample");
		}
		if (!m_compositeShader) {
			m_compositeShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("necomposite");
		}
		if (!m_selectionCompositeShader) {
			m_selectionCompositeShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("neselectioncomposite");
		}
#ifndef PRODUCTION_BUILD
		if (!m_selectionMaskFromPickingShader) {
			m_selectionMaskFromPickingShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("neselectionmaskfrompicking");
		}
#endif
		if (!m_SSAOShader) {
			m_SSAOShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nessao");
		}
		if (!m_taaShader) {
			m_taaShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("netaa");
		}
		if (!m_ssrShader) {
			m_ssrShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nessr");
		}
		if (!m_ssrResolveShader) {
			m_ssrResolveShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nessrresolve");
		}
		if (!m_ssrHiZBuildShader) {
			m_ssrHiZBuildShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nessrhizbuild");
		}
		if (!m_ssrTemporalShader) {
			m_ssrTemporalShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nessrtemporal");
		}
		if (!m_ssrApplyShader) {
			m_ssrApplyShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nessrapply");
		}
	}

	void PostProcessPipeline::SetupGraph(RenderViewHandle sourceView,
		RenderViewHandle destView,
		const Math::Mat4& invProj,
		const Math::Mat4& currView,
		const Math::Mat4& currProj,
		bool isSceneView)
	{
		if (!m_rvm || !m_graph) return;

		m_graph->Clear();

		auto sourceFB = m_rvm->GetFramebuffer(sourceView);
		auto destFB = m_rvm->GetFramebuffer(destView);
		if (!sourceFB || !destFB) return;

		m_context.sourceFB = sourceFB;
		m_context.destFB = destFB;
		m_context.invProj = invProj;
		m_context.isSceneView = isSceneView;
		m_context.currViewProjInv = (currProj * currView).Inverse();
		m_context.taaHasHistory = false;
		m_context.taaHistoryTex = 0;

		auto sceneColor = m_graph->ImportTexture(sourceFB->GetColorAttachment(), "SceneHDR");
		auto sceneDepth = m_graph->ImportTexture(sourceFB->GetDepthAttachment(), "SceneDepth");
		auto finalTex = m_graph->ImportTexture(destFB->GetColorAttachment(), "FinalOutput");

		RenderGraphResource selectionMask{};
		RenderGraphResource pickingTex{};
		bool canBuildMaskFromPicking = false;
#ifndef PRODUCTION_BUILD
		canBuildMaskFromPicking = isSceneView
			&& m_selectionSettings
			&& m_selectionSettings->enabled
			&& m_selectionCompositeShader
			&& m_selectionMaskFromPickingShader
			&& m_selectedEntityIds
			&& !m_selectedEntityIds->empty()
			&& sourceFB->HasPickingAttachment()
			&& sourceFB->GetPickingAttachment() != 0;
		if (canBuildMaskFromPicking) {
			pickingTex = m_graph->ImportTexture(sourceFB->GetPickingAttachment(), "Picking");
		}
#endif

		auto addSelectionMaskFromPickingPass = [&](const char* passName, RenderGraphResource pickingInput) {
			TextureDesc maskDesc;
			maskDesc.width = sourceFB->GetWidth();
			maskDesc.height = sourceFB->GetHeight();
			maskDesc.format = TextureFormat::R8;
			maskDesc.name = "SelectionMask";
			auto maskOutput = m_graph->CreateTexture(maskDesc);

#ifndef PRODUCTION_BUILD
			std::vector<uint32_t> selectedIdsCpu;
			selectedIdsCpu.reserve(std::min<size_t>(m_selectedEntityIds ? m_selectedEntityIds->size() : 0, 256));
			if (m_selectedEntityIds) {
				for (uint32_t id : *m_selectedEntityIds) {
					if (selectedIdsCpu.size() >= 256) break;
					selectedIdsCpu.push_back(id);
				}
			}
			const int selectedCount = static_cast<int>(selectedIdsCpu.size());
#endif

			m_graph->AddPass(passName)
				.Read(pickingInput)
				.Write(maskOutput)
				.Execute([this, pickingInput, maskOutput
#ifndef PRODUCTION_BUILD
					, selectedIdsCpu, selectedCount
#endif
				](const RenderGraphContext& ctx) {
#ifndef PRODUCTION_BUILD
					if (!m_selectionSettings || !m_selectionMaskFromPickingShader || !ctx.graph) return;
					if (selectedCount <= 0) return;

					auto& pctx = m_context;
					const uint32_t w = pctx.sourceFB->GetWidth();
					const uint32_t h = pctx.sourceFB->GetHeight();
					if (w == 0 || h == 0) return;
					const uint32_t targetFbo = ctx.graph->GetFramebufferId(maskOutput);
					if (targetFbo == 0) return;

					if (m_selectedIdsSSBO == 0) {
						glGenBuffers(1, &m_selectedIdsSSBO);
						m_selectedIdsCapacity = 0;
					}

					const size_t neededBytes = selectedIdsCpu.size() * sizeof(uint32_t);
					glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_selectedIdsSSBO);
					if (neededBytes > m_selectedIdsCapacity) {
						glBufferData(GL_SHADER_STORAGE_BUFFER, neededBytes, selectedIdsCpu.data(), GL_DYNAMIC_DRAW);
						m_selectedIdsCapacity = neededBytes;
					}
					else if (neededBytes > 0) {
						glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, neededBytes, selectedIdsCpu.data());
					}
					glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_selectedIdsSSBO);
					glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

					const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
					const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
					GLboolean depthMaskWasEnabled = GL_TRUE;
					glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);

					glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
					glViewport(0, 0, static_cast<GLint>(w), static_cast<GLint>(h));

					const float clearColor[4] = { 0, 0, 0, 0 };
					glClearBufferfv(GL_COLOR, 0, clearColor);

					glDisable(GL_DEPTH_TEST);
					glDisable(GL_BLEND);
					glDepthMask(GL_FALSE);

					m_selectionMaskFromPickingShader->Bind();
					m_selectionMaskFromPickingShader->SetUniformInt("u_Picking", 0);
					m_selectionMaskFromPickingShader->SetUniformInt("u_SelectedCount", selectedCount);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(pickingInput));

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
					glBindVertexArray(0);

					glDepthMask(depthMaskWasEnabled);
					if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
					if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
#else
					(void)ctx;
					(void)this;
					(void)pickingInput;
					(void)maskOutput;
#endif
				});

			return maskOutput;
		};

		auto addSelectionCompositePass = [&](const char* passName, RenderGraphResource inputColor) {
			TextureDesc selectionCompositeDesc;
			selectionCompositeDesc.width = sourceFB->GetWidth();
			selectionCompositeDesc.height = sourceFB->GetHeight();
			selectionCompositeDesc.format = TextureFormat::RGBA8;
			selectionCompositeDesc.name = "SelectionCompositeOutput";
			auto selectionCompositeOutput = m_graph->CreateTexture(selectionCompositeDesc);

			m_graph->AddPass(passName)
				.Read(inputColor)
				.Read(selectionMask)
				.Write(selectionCompositeOutput)
				.Execute([this, inputColor, selectionMask, selectionCompositeOutput](const RenderGraphContext& ctx) {
					if (!m_selectionSettings || !m_selectionCompositeShader || !ctx.graph) return;

					auto& pctx = m_context;
					const uint32_t w = pctx.sourceFB->GetWidth();
					const uint32_t h = pctx.sourceFB->GetHeight();
					if (w == 0 || h == 0) return;
					const uint32_t targetFbo = ctx.graph->GetFramebufferId(selectionCompositeOutput);
					if (targetFbo == 0) return;

					const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
					const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
					GLboolean depthMaskWasEnabled = GL_TRUE;
					glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);

					glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
					glViewport(0, 0, static_cast<GLint>(w), static_cast<GLint>(h));
#ifndef PRODUCTION_BUILD
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);
#endif

					glDisable(GL_DEPTH_TEST);
					glDisable(GL_BLEND);
					glDepthMask(GL_FALSE);

					m_selectionCompositeShader->Bind();
					m_selectionCompositeShader->SetUniformInt("u_SceneColor", 0);
					m_selectionCompositeShader->SetUniformInt("u_SelectionMask", 1);
					m_selectionCompositeShader->SetUniformVec2("u_TexelSize", {
						1.0f / static_cast<float>(w),
						1.0f / static_cast<float>(h)
					});
					m_selectionCompositeShader->SetUniformVec4("u_OutlineColor", m_selectionSettings->outlineColor);
					m_selectionCompositeShader->SetUniformFloat("u_OutlineThicknessPx", m_selectionSettings->outlineThicknessPx);
					m_selectionCompositeShader->SetUniformFloat("u_OutlineOpacity", m_selectionSettings->outlineOpacity);
					m_selectionCompositeShader->SetUniformFloat("u_OutlineSoftness", m_selectionSettings->outlineSoftness);
					m_selectionCompositeShader->SetUniformInt("u_FillEnabled", m_selectionSettings->fillEnabled ? 1 : 0);
					m_selectionCompositeShader->SetUniformVec4("u_FillColor", m_selectionSettings->fillColor);
					m_selectionCompositeShader->SetUniformFloat("u_FillIntensity", m_selectionSettings->fillIntensity);
					m_selectionCompositeShader->SetUniformInt("u_DebugShowMask", m_selectionSettings->debugShowMask ? 1 : 0);
					m_selectionCompositeShader->SetUniformInt("u_DebugOutlineOnly", m_selectionSettings->debugOutlineOnly ? 1 : 0);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(inputColor));
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(selectionMask));

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
					glBindVertexArray(0);

					glDepthMask(depthMaskWasEnabled);
					if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
					if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
				});

			return selectionCompositeOutput;
		};

		auto addFinalCopyPass = [&](const char* passName, RenderGraphResource inputColor) {
			m_graph->AddPass(passName)
				.Read(inputColor)
				.Write(finalTex)
				.Execute([this, inputColor](const RenderGraphContext& ctx) {
					if (!ctx.graph) return;

					const uint32_t srcFbo = ctx.graph->GetFramebufferId(inputColor);
					if (srcFbo == 0) return;

					auto& pctx = m_context;
					glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pctx.destFB->GetFramebuffer());
					glBlitFramebuffer(
						0, 0, static_cast<GLint>(pctx.sourceFB->GetWidth()), static_cast<GLint>(pctx.sourceFB->GetHeight()),
						0, 0, static_cast<GLint>(pctx.destFB->GetWidth()), static_cast<GLint>(pctx.destFB->GetHeight()),
						GL_COLOR_BUFFER_BIT, GL_LINEAR
					);
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
				});
		};

		const bool postEnabled = m_settings && m_settings->enabled;
		if (!postEnabled) {
			if (canBuildMaskFromPicking) {
				selectionMask = addSelectionMaskFromPickingPass("Selection Mask From Picking", pickingTex);
				auto selectionComposite = addSelectionCompositePass("Selection Composite", sceneColor);
				addFinalCopyPass("Final Copy", selectionComposite);
			} else {
				auto sourceFBRes = m_graph->ImportFramebuffer(sourceFB.get(), "BypassSourceFB");
				auto destFBRes = m_graph->ImportFramebuffer(destFB.get(), "BypassDestFB");

				m_graph->AddPass("Bypass Copy")
					.Read(sourceFBRes)
					.Write(destFBRes)
					.Execute([sourceFBRes, destFBRes](const RenderGraphContext& ctx) {
						if (!ctx.graph) return;

						const uint32_t srcFbo = ctx.graph->GetFramebufferId(sourceFBRes);
						const uint32_t dstFbo = ctx.graph->GetFramebufferId(destFBRes);
						auto* srcFB = ctx.GetFramebuffer(sourceFBRes);
						auto* dstFB = ctx.GetFramebuffer(destFBRes);
						if (srcFbo == 0 || dstFB == nullptr || srcFB == nullptr) return;

						glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
						glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
						glBlitFramebuffer(
							0, 0, static_cast<GLint>(srcFB->GetWidth()), static_cast<GLint>(srcFB->GetHeight()),
							0, 0, static_cast<GLint>(dstFB->GetWidth()), static_cast<GLint>(dstFB->GetHeight()),
							GL_COLOR_BUFFER_BIT, GL_LINEAR
						);
						glBindFramebuffer(GL_FRAMEBUFFER, 0);
					});
			}

			m_graph->Compile();
			return;
		}
		const bool hasMiniGBuffer = sourceFB->HasMiniGBuffer();
		const bool hasNormalAttachment = hasMiniGBuffer && sourceFB->GetNormalAttachment() != 0;
		const bool hasRoughnessAttachment = hasMiniGBuffer && sourceFB->GetRoughnessAttachment() != 0;
		RenderGraphResource sceneNormal{};
		RenderGraphResource sceneRoughness{};
		if (hasNormalAttachment) {
			sceneNormal = m_graph->ImportTexture(sourceFB->GetNormalAttachment(), "SceneNormal");
		}
		if (hasRoughnessAttachment) {
			sceneRoughness = m_graph->ImportTexture(sourceFB->GetRoughnessAttachment(), "SceneRoughness");
		}

		const bool ssrEnabled = m_settings
			&& m_settings->ssrSettings.enabled
			&& m_ssrShader
			&& m_ssrResolveShader
			&& m_ssrApplyShader
			&& sourceFB->HasDepth()
			&& sourceFB->GetDepthAttachment() != 0
			&& hasNormalAttachment
			&& hasRoughnessAttachment;

		if (ssrEnabled) {
			EnsureSSRSceneMipResources(sourceFB->GetColorAttachment(), sourceFB->GetWidth(), sourceFB->GetHeight());
			if (m_ssrSceneMipTex == 0) {
				LOG_WARNING("SSR skipped: dedicated scene mip texture is unavailable.");
			}
			else {
			const float ssrHalfScale = std::clamp(m_settings->ssrSettings.halfResScale, 0.25f, 1.0f);
			const uint32_t ssrRawW = std::max(1u, static_cast<uint32_t>(std::floor(sourceFB->GetWidth() * ssrHalfScale)));
			const uint32_t ssrRawH = std::max(1u, static_cast<uint32_t>(std::floor(sourceFB->GetHeight() * ssrHalfScale)));
			const int ssrResolveTapCount = (m_settings->ssrSettings.resolveTapCount >= 8) ? 8 : 4;

			TextureDesc ssrRawDesc;
			ssrRawDesc.width = ssrRawW;
			ssrRawDesc.height = ssrRawH;
			ssrRawDesc.format = TextureFormat::RGBA16F;
			ssrRawDesc.name = "SSRRawHalf";
			auto ssrRawTex = m_graph->CreateTexture(ssrRawDesc);

			TextureDesc ssrResolvedDesc;
			ssrResolvedDesc.width = sourceFB->GetWidth();
			ssrResolvedDesc.height = sourceFB->GetHeight();
			ssrResolvedDesc.format = TextureFormat::RGBA16F;
			ssrResolvedDesc.name = "SSRResolved";
			auto ssrResolvedTex = m_graph->CreateTexture(ssrResolvedDesc);

			const auto ssrSceneInput = m_graph->ImportTexture(m_ssrSceneMipTex, "SSRSceneMipSource");
			RenderGraphResource ssrHiZ{};
			const bool hizRequested = m_settings && m_settings->ssrSettings.hizEnabled;
			const bool hizCanBuild = hizRequested && m_ssrHiZBuildShader != nullptr;
			if (hizCanBuild) {
				EnsureSSRHiZResources(sourceFB->GetWidth(), sourceFB->GetHeight());
				if (m_ssrHiZTex != 0) {
					ssrHiZ = m_graph->ImportTexture(m_ssrHiZTex, "SSRHiZ");
					const uint32_t hizW = m_ssrHiZWidth;
					const uint32_t hizH = m_ssrHiZHeight;
					const int hizLevels = m_ssrHiZLevels;

					m_graph->AddPass("SSR: Build HiZ")
						.Read(sceneDepth)
						.Write(ssrHiZ)
						.Execute([this, sceneDepth, ssrHiZ, hizW, hizH, hizLevels](const RenderGraphContext& ctx) {
							if (!ctx.graph || !m_ssrHiZBuildShader) return;
							if (hizW == 0 || hizH == 0 || hizLevels <= 0) return;

							const uint32_t depthTex = ctx.GetTexture(sceneDepth);
							const uint32_t hizTex = ctx.GetTexture(ssrHiZ);
							if (depthTex == 0 || hizTex == 0) return;

							m_ssrHiZBuildShader->Bind();
							m_ssrHiZBuildShader->SetUniformInt("u_Depth", 0);
							m_ssrHiZBuildShader->SetUniformInt("u_HiZSampler", 1);

							glActiveTexture(GL_TEXTURE0);
							glBindTexture(GL_TEXTURE_2D, depthTex);
							glActiveTexture(GL_TEXTURE1);
							glBindTexture(GL_TEXTURE_2D, hizTex);

							auto DispatchFor = [](uint32_t w, uint32_t h) {
								const GLuint groupsX = (w + 15u) / 16u;
								const GLuint groupsY = (h + 15u) / 16u;
								glDispatchCompute(groupsX ? groupsX : 1u, groupsY ? groupsY : 1u, 1);
							};

							// Copy scene depth -> HiZ mip0
							glBindImageTexture(0, hizTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
							m_ssrHiZBuildShader->SetUniformInt("u_Mode", 0);
							m_ssrHiZBuildShader->SetUniformInt("u_SrcMip", 0);
							DispatchFor(hizW, hizH);
							glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

							// Downsample chain
							for (int mip = 0; mip < hizLevels - 1; ++mip) {
								const uint32_t dstW = std::max(1u, hizW >> (mip + 1));
								const uint32_t dstH = std::max(1u, hizH >> (mip + 1));
								glBindImageTexture(0, hizTex, mip + 1, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
								m_ssrHiZBuildShader->SetUniformInt("u_Mode", 1);
								m_ssrHiZBuildShader->SetUniformInt("u_SrcMip", mip);
								DispatchFor(dstW, dstH);
								glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
							}

							glBindTexture(GL_TEXTURE_2D, 0);
						});
				}
				else {
					LOG_WARNING("SSR HiZ disabled: unable to allocate Hi-Z texture.");
				}
			}
			else if (hizRequested) {
				LOG_WARNING("SSR HiZ disabled: missing compute shader.");
			}

			m_graph->AddPass("SSR: Copy Scene To Mip Source")
				.Read(sceneColor)
				.Write(ssrSceneInput)
				.Execute([sceneColor, ssrSceneInput, sourceW = sourceFB->GetWidth(), sourceH = sourceFB->GetHeight()](const RenderGraphContext& ctx) {
					if (!ctx.graph || sourceW == 0 || sourceH == 0) return;
					const uint32_t srcTex = ctx.GetTexture(sceneColor);
					const uint32_t dstTex = ctx.GetTexture(ssrSceneInput);
					if (srcTex == 0 || dstTex == 0) return;

					glCopyImageSubData(
						srcTex, GL_TEXTURE_2D, 0, 0, 0, 0,
						dstTex, GL_TEXTURE_2D, 0, 0, 0, 0,
						static_cast<GLsizei>(sourceW),
						static_cast<GLsizei>(sourceH),
						1
					);
				});

			m_graph->AddPass("SSR: Build Scene Mips")
				.Write(ssrSceneInput)
				.Execute([ssrSceneInput, sourceW = sourceFB->GetWidth(), sourceH = sourceFB->GetHeight()](const RenderGraphContext& ctx) {
					if (!ctx.graph) return;
					const uint32_t sceneTex = ctx.GetTexture(ssrSceneInput);
					if (sceneTex == 0) return;
					if (sourceW == 0 || sourceH == 0) {
						LOG_WARNING("SSR mip generation skipped: invalid source dimensions.");
						return;
					}

					glBindTexture(GL_TEXTURE_2D, sceneTex);
					glGenerateMipmap(GL_TEXTURE_2D);
					glBindTexture(GL_TEXTURE_2D, 0);
				});

			m_graph->AddPass("SSR: Trace")
				.Read(ssrSceneInput)
				.Read(sceneDepth)
				.Read(sceneNormal)
				.Read(sceneRoughness)
				.Read(ssrHiZ)
				.Write(ssrRawTex)
				.Execute([this, ssrSceneInput, sceneDepth, sceneNormal, sceneRoughness, ssrHiZ, currView, currProj, ssrRawTex, ssrRawW, ssrRawH](const RenderGraphContext& ctx) {
					if (!m_settings || !m_ssrShader || !ctx.graph) return;

					auto& pctx = m_context;
					const uint32_t fullW = pctx.sourceFB->GetWidth();
					const uint32_t fullH = pctx.sourceFB->GetHeight();
					if (fullW == 0 || fullH == 0 || ssrRawW == 0 || ssrRawH == 0) return;

					const uint32_t targetFbo = ctx.graph->GetFramebufferId(ssrRawTex);
					if (targetFbo == 0) return;

					const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
					glDisable(GL_DEPTH_TEST);

					glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
					glViewport(0, 0, static_cast<GLint>(ssrRawW), static_cast<GLint>(ssrRawH));
#ifndef PRODUCTION_BUILD
					glClearColor(0, 0, 0, 0);
					glClear(GL_COLOR_BUFFER_BIT);
#endif

					const float maxSceneMip = std::floor(std::log2(static_cast<float>(std::max(fullW, fullH))));

					m_ssrShader->Bind();
					m_ssrShader->SetUniformInt("u_SceneColor", 0);
					m_ssrShader->SetUniformInt("u_Depth", 1);
					m_ssrShader->SetUniformInt("u_Normal", 2);
					m_ssrShader->SetUniformInt("u_Roughness", 3);
					m_ssrShader->SetUniformMat4("u_InvProj", pctx.invProj);
					m_ssrShader->SetUniformMat4("u_Proj", currProj);
					m_ssrShader->SetUniformMat4("u_View", currView);
					m_ssrShader->SetUniformFloat("u_MaxDistance", m_settings->ssrSettings.maxDistance);
					m_ssrShader->SetUniformFloat("u_Thickness", m_settings->ssrSettings.thickness);
					m_ssrShader->SetUniformInt("u_MaxSteps", m_settings->ssrSettings.maxSteps);
					m_ssrShader->SetUniformInt("u_FrameIndex", static_cast<int>(m_frameIndex & 0x7fffffff));
					m_ssrShader->SetUniformFloat("u_Stride", m_settings->ssrSettings.stride);
					m_ssrShader->SetUniformInt("u_BinarySearchSteps", m_settings->ssrSettings.binarySearchSteps);
					m_ssrShader->SetUniformFloat("u_RoughnessCutoff", m_settings->ssrSettings.roughnessCutoff);
					m_ssrShader->SetUniformFloat("u_EdgeFade", m_settings->ssrSettings.edgeFade);
					m_ssrShader->SetUniformFloat("u_MaxSceneMip", maxSceneMip);

					const uint32_t hizTex = ctx.GetTexture(ssrHiZ);
					const bool hizEnabled = m_settings->ssrSettings.hizEnabled && (hizTex != 0);
					m_ssrShader->SetUniformInt("u_HiZ", 4);
					m_ssrShader->SetUniformInt("u_HiZEnabled", hizEnabled ? 1 : 0);
					m_ssrShader->SetUniformInt("u_HiZStartMip", m_settings->ssrSettings.hizStartMip);
					m_ssrShader->SetUniformFloat("u_HiZDepthBias", m_settings->ssrSettings.hizDepthBias);
					m_ssrShader->SetUniformInt("u_HiZRefineSteps", m_settings->ssrSettings.hizRefineSteps);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(ssrSceneInput));
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneDepth));
					glActiveTexture(GL_TEXTURE2);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneNormal));
					glActiveTexture(GL_TEXTURE3);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneRoughness));
					glActiveTexture(GL_TEXTURE4);
					glBindTexture(GL_TEXTURE_2D, hizEnabled ? hizTex : 0);

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
					glBindVertexArray(0);

					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
				});

			m_graph->AddPass("SSR: Resolve")
				.Read(ssrRawTex)
				.Read(sceneDepth)
				.Read(sceneNormal)
				.Read(sceneRoughness)
				.Write(ssrResolvedTex)
				.Execute([this, ssrRawTex, sceneDepth, sceneNormal, sceneRoughness, currView, ssrResolvedTex, ssrResolveTapCount](const RenderGraphContext& ctx) {
					if (!m_settings || !m_ssrResolveShader || !ctx.graph) return;

					auto& pctx = m_context;
					const uint32_t w = pctx.sourceFB->GetWidth();
					const uint32_t h = pctx.sourceFB->GetHeight();
					if (w == 0 || h == 0) return;

					const uint32_t targetFbo = ctx.graph->GetFramebufferId(ssrResolvedTex);
					if (targetFbo == 0) return;

					const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
					glDisable(GL_DEPTH_TEST);

					glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
					glViewport(0, 0, w, h);
#ifndef PRODUCTION_BUILD
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);
#endif

					m_ssrResolveShader->Bind();
					m_ssrResolveShader->SetUniformInt("u_SSRRaw", 0);
					m_ssrResolveShader->SetUniformInt("u_Depth", 1);
					m_ssrResolveShader->SetUniformInt("u_Normal", 2);
					m_ssrResolveShader->SetUniformInt("u_Roughness", 3);
					m_ssrResolveShader->SetUniformMat4("u_InvProj", pctx.invProj);
					m_ssrResolveShader->SetUniformMat4("u_View", currView);
					m_ssrResolveShader->SetUniformInt("u_ResolveTapCount", ssrResolveTapCount);
					m_ssrResolveShader->SetUniformFloat("u_Intensity", m_settings->ssrSettings.intensity);
					m_ssrResolveShader->SetUniformFloat("u_RoughnessCutoff", m_settings->ssrSettings.roughnessCutoff);
					m_ssrResolveShader->SetUniformFloat("u_FresnelPower", m_settings->ssrSettings.fresnelPower);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(ssrRawTex));
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneDepth));
					glActiveTexture(GL_TEXTURE2);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneNormal));
					glActiveTexture(GL_TEXTURE3);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneRoughness));

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
					glBindVertexArray(0);

					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
				});

			RenderGraphResource ssrContributionTex = ssrResolvedTex;
			const bool ssrTemporalEnabled = m_settings->ssrSettings.temporalEnabled && m_ssrTemporalShader;
			if (ssrTemporalEnabled) {
				EnsureSSRTemporalResources(
					sourceView,
					sourceFB->GetWidth(),
					sourceFB->GetHeight(),
					sourceFB->GetDepthAttachment(),
					sourceFB->GetNormalAttachment(),
					sourceFB->GetRoughnessAttachment()
				);
				auto temporalIt = m_ssrTemporalStates.find(sourceView);
				if (temporalIt != m_ssrTemporalStates.end()) {
					auto& temporalState = temporalIt->second;
					const float projResetThreshold = std::max(0.0f, m_settings->ssrSettings.temporalProjectionResetThreshold);
					if (temporalState.hasHistory && MaxAbsMatrixDelta(currProj, temporalState.prevProj) > projResetThreshold) {
						temporalState.hasHistory = false;
					}

					const int ssrHistoryReadIndex = temporalState.readIndex;
					const int ssrHistoryWriteIndex = temporalState.writeIndex;
					auto historyRead = m_graph->ImportTexture(temporalState.historyColorTex[ssrHistoryReadIndex], "SSRHistoryRead");
					auto historyWrite = m_graph->ImportTexture(temporalState.historyColorTex[ssrHistoryWriteIndex], "SSRHistoryWrite");
					auto prevDepthRead = m_graph->ImportTexture(temporalState.historyDepthTex[ssrHistoryReadIndex], "SSRHistoryPrevDepth");
					auto prevDepthWrite = m_graph->ImportTexture(temporalState.historyDepthTex[ssrHistoryWriteIndex], "SSRHistoryWriteDepth");
					auto prevNormalRead = m_graph->ImportTexture(temporalState.historyNormalTex[ssrHistoryReadIndex], "SSRHistoryPrevNormal");
					auto prevNormalWrite = m_graph->ImportTexture(temporalState.historyNormalTex[ssrHistoryWriteIndex], "SSRHistoryWriteNormal");
					auto prevRoughnessRead = m_graph->ImportTexture(temporalState.historyRoughnessTex[ssrHistoryReadIndex], "SSRHistoryPrevRoughness");
					auto prevRoughnessWrite = m_graph->ImportTexture(temporalState.historyRoughnessTex[ssrHistoryWriteIndex], "SSRHistoryWriteRoughness");

					TextureDesc ssrTemporalDesc;
					ssrTemporalDesc.width = sourceFB->GetWidth();
					ssrTemporalDesc.height = sourceFB->GetHeight();
					ssrTemporalDesc.format = TextureFormat::RGBA16F;
					ssrTemporalDesc.name = "SSRTemporal";
					auto ssrTemporalTex = m_graph->CreateTexture(ssrTemporalDesc);

					m_graph->AddPass("SSR: Temporal")
						.Read(ssrResolvedTex)
						.Read(historyRead)
						.Read(prevDepthRead)
						.Read(prevNormalRead)
						.Read(prevRoughnessRead)
						.Read(sceneDepth)
						.Read(sceneNormal)
						.Read(sceneRoughness)
						.Write(ssrTemporalTex)
						.Execute([this, sourceView, ssrResolvedTex, historyRead, prevDepthRead, prevNormalRead, prevRoughnessRead, sceneDepth, sceneNormal, sceneRoughness, ssrTemporalTex](const RenderGraphContext& ctx) {
							if (!m_settings || !m_ssrTemporalShader || !ctx.graph) return;
							auto it = m_ssrTemporalStates.find(sourceView);
							if (it == m_ssrTemporalStates.end()) return;

							auto& pctx = m_context;
							auto& state = it->second;
							const uint32_t w = pctx.sourceFB->GetWidth();
							const uint32_t h = pctx.sourceFB->GetHeight();
							if (w == 0 || h == 0) return;

							const uint32_t targetFbo = ctx.graph->GetFramebufferId(ssrTemporalTex);
							if (targetFbo == 0) return;

							const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
							glDisable(GL_DEPTH_TEST);

							glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
							glViewport(0, 0, static_cast<GLint>(w), static_cast<GLint>(h));
#ifndef PRODUCTION_BUILD
							glClearColor(0, 0, 0, 0);
							glClear(GL_COLOR_BUFFER_BIT);
#endif

							m_ssrTemporalShader->Bind();
							m_ssrTemporalShader->SetUniformInt("u_CurrentSSR", 0);
							m_ssrTemporalShader->SetUniformInt("u_HistorySSR", 1);
							m_ssrTemporalShader->SetUniformInt("u_Depth", 2);
							m_ssrTemporalShader->SetUniformInt("u_Normal", 3);
							m_ssrTemporalShader->SetUniformInt("u_Roughness", 4);
							m_ssrTemporalShader->SetUniformInt("u_PrevDepth", 5);
							m_ssrTemporalShader->SetUniformInt("u_PrevNormal", 6);
							m_ssrTemporalShader->SetUniformInt("u_PrevRoughness", 7);
							m_ssrTemporalShader->SetUniformInt("u_HasHistory", state.hasHistory ? 1 : 0);
							m_ssrTemporalShader->SetUniformMat4("u_CurrViewProjInv", pctx.currViewProjInv);
							m_ssrTemporalShader->SetUniformMat4("u_PrevViewProj", state.prevViewProj);
							m_ssrTemporalShader->SetUniformVec2("u_TexelSize", { 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h) });
							m_ssrTemporalShader->SetUniformFloat("u_RoughnessCutoff", m_settings->ssrSettings.roughnessCutoff);
							m_ssrTemporalShader->SetUniformFloat("u_HistoryBlendMin", m_settings->ssrSettings.temporalHistoryMin);
							m_ssrTemporalShader->SetUniformFloat("u_HistoryBlendMax", m_settings->ssrSettings.temporalHistoryMax);
							m_ssrTemporalShader->SetUniformFloat("u_NormalRejectDot", m_settings->ssrSettings.temporalNormalRejectDot);
							m_ssrTemporalShader->SetUniformFloat("u_RoughnessReject", m_settings->ssrSettings.temporalRoughnessReject);
							m_ssrTemporalShader->SetUniformFloat("u_DepthReject", m_settings->ssrSettings.temporalDepthReject);

							glActiveTexture(GL_TEXTURE0);
							glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(ssrResolvedTex));
							glActiveTexture(GL_TEXTURE1);
							glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(historyRead));
							glActiveTexture(GL_TEXTURE2);
							glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneDepth));
							glActiveTexture(GL_TEXTURE3);
							glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneNormal));
							glActiveTexture(GL_TEXTURE4);
							glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneRoughness));
							glActiveTexture(GL_TEXTURE5);
							glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(prevDepthRead));
							glActiveTexture(GL_TEXTURE6);
							glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(prevNormalRead));
							glActiveTexture(GL_TEXTURE7);
							glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(prevRoughnessRead));

							glBindVertexArray(m_QuadVAO);
							glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
							glBindVertexArray(0);

							glBindFramebuffer(GL_FRAMEBUFFER, 0);
							if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
						});

					m_graph->AddPass("SSR: Commit History")
						.Read(ssrTemporalTex)
						.Write(historyWrite)
						.Write(prevDepthWrite)
						.Write(prevNormalWrite)
						.Write(prevRoughnessWrite)
						.Read(sceneDepth)
						.Read(sceneNormal)
						.Read(sceneRoughness)
						.Execute([this, sourceView, currView, currProj, ssrTemporalTex, historyWrite, prevDepthWrite, prevNormalWrite, prevRoughnessWrite, sceneDepth, sceneNormal, sceneRoughness, ssrHistoryReadIndex, ssrHistoryWriteIndex](const RenderGraphContext& ctx) {
							if (!ctx.graph) return;
							auto it = m_ssrTemporalStates.find(sourceView);
							if (it == m_ssrTemporalStates.end()) return;

							auto& state = it->second;
							const uint32_t srcTex = ctx.GetTexture(ssrTemporalTex);
							const uint32_t dstTex = ctx.GetTexture(historyWrite);
							if (srcTex == 0 || dstTex == 0 || state.width == 0 || state.height == 0) return;

							glCopyImageSubData(
								srcTex, GL_TEXTURE_2D, 0, 0, 0, 0,
								dstTex, GL_TEXTURE_2D, 0, 0, 0, 0,
								static_cast<GLsizei>(state.width),
								static_cast<GLsizei>(state.height),
								1
							);

							const uint32_t currDepthTex = ctx.GetTexture(sceneDepth);
							const uint32_t currNormalTex = ctx.GetTexture(sceneNormal);
							const uint32_t currRoughnessTex = ctx.GetTexture(sceneRoughness);
							const uint32_t dstDepthTex = ctx.GetTexture(prevDepthWrite);
							const uint32_t dstNormalTex = ctx.GetTexture(prevNormalWrite);
							const uint32_t dstRoughnessTex = ctx.GetTexture(prevRoughnessWrite);
							if (currDepthTex != 0 && dstDepthTex != 0) {
								glCopyImageSubData(
									currDepthTex, GL_TEXTURE_2D, 0, 0, 0, 0,
									dstDepthTex, GL_TEXTURE_2D, 0, 0, 0, 0,
									static_cast<GLsizei>(state.width),
									static_cast<GLsizei>(state.height),
									1
								);
							}
							if (currNormalTex != 0 && dstNormalTex != 0) {
								glCopyImageSubData(
									currNormalTex, GL_TEXTURE_2D, 0, 0, 0, 0,
									dstNormalTex, GL_TEXTURE_2D, 0, 0, 0, 0,
									static_cast<GLsizei>(state.width),
									static_cast<GLsizei>(state.height),
									1
								);
							}
							if (currRoughnessTex != 0 && dstRoughnessTex != 0) {
								glCopyImageSubData(
									currRoughnessTex, GL_TEXTURE_2D, 0, 0, 0, 0,
									dstRoughnessTex, GL_TEXTURE_2D, 0, 0, 0, 0,
									static_cast<GLsizei>(state.width),
									static_cast<GLsizei>(state.height),
									1
								);
							}

							state.prevViewProj = currProj * currView;
							state.prevProj = currProj;
							state.hasHistory = true;
							state.readIndex = ssrHistoryWriteIndex;
							state.writeIndex = ssrHistoryReadIndex;
						});

					ssrContributionTex = ssrTemporalTex;
				}
			}
			else {
				auto temporalIt = m_ssrTemporalStates.find(sourceView);
				if (temporalIt != m_ssrTemporalStates.end()) {
					temporalIt->second.hasHistory = false;
				}
			}

			TextureDesc ssrAppliedDesc;
			ssrAppliedDesc.width = sourceFB->GetWidth();
			ssrAppliedDesc.height = sourceFB->GetHeight();
			ssrAppliedDesc.format = TextureFormat::RGBA16F;
			ssrAppliedDesc.name = "SSRApplied";
			auto ssrAppliedTex = m_graph->CreateTexture(ssrAppliedDesc);

			m_graph->AddPass("SSR: Apply")
				.Read(sceneColor)
				.Read(ssrContributionTex)
				.Write(ssrAppliedTex)
				.Execute([this, sceneColor, ssrContributionTex, ssrAppliedTex](const RenderGraphContext& ctx) {
					if (!m_ssrApplyShader || !ctx.graph) return;

					auto& pctx = m_context;
					const uint32_t w = pctx.sourceFB->GetWidth();
					const uint32_t h = pctx.sourceFB->GetHeight();
					if (w == 0 || h == 0) return;

					const uint32_t targetFbo = ctx.graph->GetFramebufferId(ssrAppliedTex);
					if (targetFbo == 0) return;

					const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
					glDisable(GL_DEPTH_TEST);

					glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
					glViewport(0, 0, static_cast<GLint>(w), static_cast<GLint>(h));
#ifndef PRODUCTION_BUILD
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);
#endif

					m_ssrApplyShader->Bind();
					m_ssrApplyShader->SetUniformInt("u_SceneColor", 0);
					m_ssrApplyShader->SetUniformInt("u_SSRContribution", 1);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneColor));
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(ssrContributionTex));

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
					glBindVertexArray(0);

					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
				});

			sceneColor = ssrAppliedTex;
			}
		}
		else {
			auto temporalIt = m_ssrTemporalStates.find(sourceView);
			if (temporalIt != m_ssrTemporalStates.end()) {
				temporalIt->second.hasHistory = false;
			}
		}

		const bool canRunTAA = m_settings
			&& m_settings->taaSettings.enabled
			&& m_taaShader
			&& sourceFB->HasDepth()
			&& sourceFB->GetDepthAttachment() != 0;

		if (canRunTAA) {
			EnsureTAAResources(sourceView, sourceFB->GetWidth(), sourceFB->GetHeight());
			auto stateIt = m_taaStates.find(sourceView);
			if (stateIt != m_taaStates.end()) {
				auto& state = stateIt->second;
				const int taaReadIndex = state.readIndex;
				const int taaWriteIndex = state.writeIndex;
				m_context.prevViewProj = state.prevViewProj;
				m_context.taaHasHistory = state.hasHistory;
				m_context.taaHistoryTex = state.historyTex[taaReadIndex];

				auto historyRead = m_graph->ImportTexture(state.historyTex[taaReadIndex], "TAAHistoryRead");
				auto historyWrite = m_graph->ImportTexture(state.historyTex[taaWriteIndex], "TAAHistoryWrite");
				const auto taaInputSceneColor = sceneColor;
				TextureDesc taaOutputDesc;
				taaOutputDesc.width = sourceFB->GetWidth();
				taaOutputDesc.height = sourceFB->GetHeight();
				taaOutputDesc.format = TextureFormat::RGBA16F;
				taaOutputDesc.name = "TAAOutput";
				auto taaOutput = m_graph->CreateTexture(taaOutputDesc);

				m_graph->AddPass("TAA")
					.Read(taaInputSceneColor)
					.Read(sceneDepth)
					.Read(historyRead)
					.Write(taaOutput)
					.Execute([this, sourceView, currView, currProj, taaInputSceneColor, sceneDepth, historyRead, taaOutput, taaReadIndex, taaWriteIndex](const RenderGraphContext& ctx) {
						if (!m_settings || !m_taaShader || !ctx.graph) return;
						auto it = m_taaStates.find(sourceView);
						if (it == m_taaStates.end()) return;

						auto& pctx = m_context;
						auto& state = it->second;
						const uint32_t w = pctx.sourceFB->GetWidth();
						const uint32_t h = pctx.sourceFB->GetHeight();
						if (w == 0 || h == 0) return;

						const uint32_t targetFbo = ctx.graph->GetFramebufferId(taaOutput);
						if (targetFbo == 0) return;

						glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
						glViewport(0, 0, w, h);
#ifndef PRODUCTION_BUILD
						glClearColor(0, 0, 0, 1);
						glClear(GL_COLOR_BUFFER_BIT);
#endif

						const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
						glDisable(GL_DEPTH_TEST);

						m_taaShader->Bind();
						m_taaShader->SetUniformInt("u_CurrentColor", 0);
						m_taaShader->SetUniformInt("u_CurrentDepth", 1);
						m_taaShader->SetUniformInt("u_History", 2);
						m_taaShader->SetUniformInt("u_HasHistory", state.hasHistory ? 1 : 0);
						m_taaShader->SetUniformFloat("u_FeedbackMin", m_settings->taaSettings.feedbackMin);
						m_taaShader->SetUniformFloat("u_FeedbackMax", m_settings->taaSettings.feedbackMax);
						m_taaShader->SetUniformFloat("u_Sharpen", m_settings->taaSettings.sharpen);
						m_taaShader->SetUniformVec2("u_TexelSize", { 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h) });
						m_taaShader->SetUniformMat4("u_CurrViewProjInv", (currProj * currView).Inverse());
						m_taaShader->SetUniformMat4("u_PrevViewProj", state.prevViewProj);

						glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(taaInputSceneColor));

						glActiveTexture(GL_TEXTURE1);
						glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneDepth));

						glActiveTexture(GL_TEXTURE2);
						glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(historyRead));

						glBindVertexArray(m_QuadVAO);
						glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
						glBindVertexArray(0);

						glBindFramebuffer(GL_FRAMEBUFFER, 0);
						if (depthWasEnabled) glEnable(GL_DEPTH_TEST);

						state.prevViewProj = currProj * currView;
						state.hasHistory = true;
						state.readIndex = taaWriteIndex;
						state.writeIndex = taaReadIndex;
					});

				m_graph->AddPass("TAA: Commit History")
					.Read(taaOutput)
					.Write(historyWrite)
					.Execute([this, sourceView, taaOutput, historyWrite](const RenderGraphContext& ctx) {
						if (!ctx.graph) return;
						auto it = m_taaStates.find(sourceView);
						if (it == m_taaStates.end()) return;

						const uint32_t srcTex = ctx.GetTexture(taaOutput);
						const uint32_t dstTex = ctx.GetTexture(historyWrite);
						if (srcTex == 0 || dstTex == 0) return;

						const auto& state = it->second;
						if (state.width == 0 || state.height == 0) return;

						glCopyImageSubData(
							srcTex, GL_TEXTURE_2D, 0, 0, 0, 0,
							dstTex, GL_TEXTURE_2D, 0, 0, 0, 0,
							static_cast<GLsizei>(state.width),
							static_cast<GLsizei>(state.height),
							1
						);
					});

				sceneColor = taaOutput;
			}
		}
		else {
			auto stateIt = m_taaStates.find(sourceView);
			if (stateIt != m_taaStates.end()) {
				stateIt->second.hasHistory = false;
			}
		}

		const bool ssaoEnabled = m_settings->ssaoSettings.enabled && m_SSAOShader;
		const bool bloomEnabled = m_settings->bloomSettings.enabled
			&& m_downSampleShader
			&& m_upSampleShader;

		RenderGraphResource ssaoTex{};
		if (ssaoEnabled) {
			TextureDesc ssaoDesc;
			ssaoDesc.width = sourceFB->GetWidth();
			ssaoDesc.height = sourceFB->GetHeight();
			ssaoDesc.format = TextureFormat::R8;
			ssaoDesc.name = "SSAO";
			ssaoTex = m_graph->CreateTexture(ssaoDesc);
		}

		auto bloomDown0 = m_graph->ImportTexture(m_bloomTempTex[0], "BloomDown0");
		auto bloomTex = m_graph->ImportTexture(m_bloomTex[0], "BloomResult");

		if (ssaoEnabled) {
			m_graph->AddPass("SSAO")
				.Read(sceneDepth)
				.Write(ssaoTex)
				.Execute([this, ssaoTex, sceneDepth](const RenderGraphContext& ctx) {
					if (!m_settings || !m_SSAOShader || !ctx.graph) return;

					auto& pctx = m_context;
					uint32_t w = pctx.sourceFB->GetWidth();
					uint32_t h = pctx.sourceFB->GetHeight();

					GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
					glDisable(GL_DEPTH_TEST);

					const uint32_t targetFbo = ctx.graph->GetFramebufferId(ssaoTex);
					if (targetFbo == 0) return;

					glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
					glViewport(0, 0, w, h);
#ifndef PRODUCTION_BUILD
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);
#endif

					m_SSAOShader->Bind();
					m_SSAOShader->SetUniformInt("u_Depth", 0);
					m_SSAOShader->SetUniformFloat("u_Radius", m_settings->ssaoSettings.radius);
					m_SSAOShader->SetUniformFloat("u_Bias", m_settings->ssaoSettings.bias);
					m_SSAOShader->SetUniformFloat("u_Intensity", m_settings->ssaoSettings.intensity);
					m_SSAOShader->SetUniformFloat("u_Power", m_settings->ssaoSettings.power);
					m_SSAOShader->SetUniformMat4("u_InvProj", pctx.invProj);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneDepth));

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
					glBindVertexArray(0);

					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
				});
		}

		if (bloomEnabled) {
			m_graph->AddPass("Bloom: Downsample")
				.Read(sceneColor)
				.Write(bloomDown0)
				.Execute([this, sceneColor](const RenderGraphContext& ctx) {
					if (!m_settings || !m_downSampleShader) return;

					auto& pctx = m_context;
					const uint32_t sceneTex = ctx.GetTexture(sceneColor);
					if (sceneTex == 0) return;

					const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
					const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
					GLboolean depthMaskWasEnabled = GL_TRUE;
					glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);
					glDisable(GL_DEPTH_TEST);
					glDepthMask(GL_FALSE);
					glDisable(GL_BLEND);

					GLuint srcTex = sceneTex;
					int srcW = static_cast<int>(pctx.sourceFB->GetWidth());
					int srcH = static_cast<int>(pctx.sourceFB->GetHeight());

					const float threshold = m_settings->bloomSettings.brightThreshold;
					const float softKnee = m_settings->bloomSettings.softKnee;
					const float scale = m_settings->bloomSettings.brightScale;
					const float fireflyStrength = 0.35f;
					const float filterRadius = std::clamp(m_settings->bloomSettings.bloomRadius, 0.25f, 3.0f);

					for (int level = 0; level < BLOOM_LEVELS; ++level) {
						const int dstW = m_bloomWidth[level];
						const int dstH = m_bloomHeight[level];

						glBindFramebuffer(GL_FRAMEBUFFER, m_bloomTempFBO[level]);
						glViewport(0, 0, dstW, dstH);

						m_downSampleShader->Bind();
						m_downSampleShader->SetUniformInt("u_Source", 0);
						m_downSampleShader->SetUniformVec2("u_TexelSize", { 1.0f / srcW, 1.0f / srcH });
						m_downSampleShader->SetUniformFloat("u_FilterRadius", filterRadius);

						const int applyThreshold = (level == 0) ? 1 : 0;
						m_downSampleShader->SetUniformInt("u_ApplyThreshold", applyThreshold);
						m_downSampleShader->SetUniformFloat("u_Threshold", applyThreshold ? threshold : 0.0f);
						m_downSampleShader->SetUniformFloat("u_SoftKnee", applyThreshold ? softKnee : 0.0f);
						m_downSampleShader->SetUniformFloat("u_Scale", applyThreshold ? scale : 1.0f);
						m_downSampleShader->SetUniformFloat("u_FireflyStrength", applyThreshold ? fireflyStrength : 0.0f);

						glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_2D, srcTex);

						glBindVertexArray(m_QuadVAO);
						glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

						srcTex = m_bloomTempTex[level];
						srcW = dstW;
						srcH = dstH;
					}

					glBindVertexArray(0);
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					glDepthMask(depthMaskWasEnabled);
					if (blendWasEnabled) glEnable(GL_BLEND);
					if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
				});

			m_graph->AddPass("Bloom: Upsample")
				.Read(bloomDown0)
				.Write(bloomTex)
				.Execute([this](const RenderGraphContext& ctx) {
					if (!m_settings || !m_upSampleShader) return;

					const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
					const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
					GLboolean depthMaskWasEnabled = GL_TRUE;
					glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);
					glDisable(GL_DEPTH_TEST);
					glDepthMask(GL_FALSE);
					glDisable(GL_BLEND);

					const float filterRadius = std::clamp(m_settings->bloomSettings.bloomRadius, 0.25f, 3.0f);

					GLuint lowTex = m_bloomTempTex[BLOOM_LEVELS - 1];
					int lowW = m_bloomWidth[BLOOM_LEVELS - 1];
					int lowH = m_bloomHeight[BLOOM_LEVELS - 1];

					for (int level = BLOOM_LEVELS - 1; level > 0; --level) {
						const int hi = level - 1;
						const int dstW = m_bloomWidth[hi];
						const int dstH = m_bloomHeight[hi];

						glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[hi]);
						glViewport(0, 0, dstW, dstH);

						const float t = (BLOOM_LEVELS > 1) ? (static_cast<float>(level) / static_cast<float>(BLOOM_LEVELS - 1)) : 1.0f;
						const float mipWeight = std::lerp(0.6f, 1.0f, std::clamp(t, 0.0f, 1.0f));

						m_upSampleShader->Bind();
						m_upSampleShader->SetUniformInt("u_LowRes", 0);
						m_upSampleShader->SetUniformInt("u_HighRes", 1);
						m_upSampleShader->SetUniformVec2("u_LowTexelSize", { 1.0f / lowW, 1.0f / lowH });
						m_upSampleShader->SetUniformFloat("u_FilterRadius", filterRadius);
						m_upSampleShader->SetUniformFloat("u_Intensity", mipWeight);

						glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_2D, lowTex);
						glActiveTexture(GL_TEXTURE1);
						glBindTexture(GL_TEXTURE_2D, m_bloomTempTex[hi]);

						glBindVertexArray(m_QuadVAO);
						glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

						lowTex = m_bloomTex[hi];
						lowW = dstW;
						lowH = dstH;
					}

					glBindVertexArray(0);
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					glDepthMask(depthMaskWasEnabled);
					if (blendWasEnabled) glEnable(GL_BLEND);
					if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
				});
		}

		TextureDesc compositeOutputDesc;
		compositeOutputDesc.width = sourceFB->GetWidth();
		compositeOutputDesc.height = sourceFB->GetHeight();
		compositeOutputDesc.format = TextureFormat::RGBA8;
		compositeOutputDesc.name = "CompositeOutput";
		auto compositeOutput = m_graph->CreateTexture(compositeOutputDesc);

		m_graph->AddPass("Composite")
			.Read(sceneColor)
			.Read(ssaoTex)
			.Read(bloomTex)
			.Write(compositeOutput)
			.Execute([this, sceneColor, ssaoTex, bloomTex, bloomEnabled, ssaoEnabled, compositeOutput](const RenderGraphContext& ctx) {
				if (!m_settings || !m_compositeShader) return;

				auto& pctx = m_context;
				uint32_t w = pctx.sourceFB->GetWidth();
				uint32_t h = pctx.sourceFB->GetHeight();

				const uint32_t targetFbo = ctx.graph->GetFramebufferId(compositeOutput);
				if (targetFbo == 0) return;

				glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
				glViewport(0, 0, w, h);
#ifndef PRODUCTION_BUILD
				glClearColor(0, 0, 0, 1);
				glClear(GL_COLOR_BUFFER_BIT);
#endif

				glDisable(GL_DEPTH_TEST);
				glDepthMask(GL_FALSE);

				m_compositeShader->Bind();
				m_compositeShader->SetUniformInt("u_SceneHDR", 0);
				m_compositeShader->SetUniformInt("u_Bloom", 1);
				m_compositeShader->SetUniformInt("u_ToneMapType", static_cast<int>(m_settings->bloomSettings.toneMapType));
				m_compositeShader->SetUniformFloat("u_BloomStrength", bloomEnabled ? m_settings->bloomSettings.bloomIntensity : 0.0f);
				m_compositeShader->SetUniformFloat("u_Exposure", m_settings->bloomSettings.exposure);

				m_compositeShader->SetUniformInt("u_SSAO", 2);
				m_compositeShader->SetUniformInt("u_UseSSAO", ssaoEnabled ? 1 : 0);
				m_compositeShader->SetUniformFloat("u_AOIntensity", m_settings->ssaoSettings.intensity);

				m_compositeShader->SetUniformInt("u_UseVignette", m_settings->vignetteSettings.enabled);
				m_compositeShader->SetUniformFloat("u_VignetteIntensity", m_settings->vignetteSettings.intensity);
				m_compositeShader->SetUniformFloat("u_VignetteRadius", m_settings->vignetteSettings.radius);
				m_compositeShader->SetUniformFloat("u_VignetteSoftness", m_settings->vignetteSettings.softness);
				m_compositeShader->SetUniformVec3("u_VignetteTint", m_settings->vignetteSettings.tint);
				m_compositeShader->SetUniformFloat("u_VignetteTintAmount", m_settings->vignetteSettings.tintIntensity);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneColor));

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, bloomEnabled ? ctx.GetTexture(bloomTex) : 0);

				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, ssaoEnabled ? ctx.GetTexture(ssaoTex) : 0);

				glBindVertexArray(m_QuadVAO);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				glBindVertexArray(0);

				glDepthMask(GL_TRUE);
				glEnable(GL_DEPTH_TEST);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			});

		if (canBuildMaskFromPicking) {
			selectionMask = addSelectionMaskFromPickingPass("Selection Mask From Picking", pickingTex);
			auto selectionComposite = addSelectionCompositePass("Selection Composite", compositeOutput);
			addFinalCopyPass("Final Copy", selectionComposite);
		}
		else {
			addFinalCopyPass("Final Copy", compositeOutput);
		}

		m_graph->Compile();
	}

	void PostProcessPipeline::DestroyResources(bool destroyQuad)
	{
#ifndef PRODUCTION_BUILD
		if (m_selectedIdsSSBO) {
			glDeleteBuffers(1, &m_selectedIdsSSBO);
			m_selectedIdsSSBO = 0;
			m_selectedIdsCapacity = 0;
		}
#endif
		if (destroyQuad) {
			if (m_QuadVBO) {
				glDeleteBuffers(1, &m_QuadVBO);
				m_QuadVBO = 0;
			}
			if (m_QuadVAO) {
				glDeleteVertexArrays(1, &m_QuadVAO);
				m_QuadVAO = 0;
			}
		}

		for (int i = 0; i < BLOOM_LEVELS; ++i) {
			if (m_bloomTex[i]) {
				glDeleteTextures(1, &m_bloomTex[i]);
				m_bloomTex[i] = 0;
			}
			if (m_bloomFBO[i]) {
				glDeleteFramebuffers(1, &m_bloomFBO[i]);
				m_bloomFBO[i] = 0;
			}
			if (m_bloomTempTex[i]) {
				glDeleteTextures(1, &m_bloomTempTex[i]);
				m_bloomTempTex[i] = 0;
			}
			if (m_bloomTempFBO[i]) {
				glDeleteFramebuffers(1, &m_bloomTempFBO[i]);
				m_bloomTempFBO[i] = 0;
			}
		}

		if (m_SSAOTex) {
			glDeleteTextures(1, &m_SSAOTex);
			m_SSAOTex = 0;
		}
		if (m_SSAOFBO) {
			glDeleteFramebuffers(1, &m_SSAOFBO);
			m_SSAOFBO = 0;
		}
		if (m_ssrSceneMipTex) {
			glDeleteTextures(1, &m_ssrSceneMipTex);
			m_ssrSceneMipTex = 0;
		}
		if (m_ssrSceneMipFBO) {
			glDeleteFramebuffers(1, &m_ssrSceneMipFBO);
			m_ssrSceneMipFBO = 0;
		}
		m_ssrSceneMipWidth = 0;
		m_ssrSceneMipHeight = 0;
		m_ssrSceneMipLevels = 1;
		m_ssrSceneMipInternalFormat = 0;

		if (m_ssrHiZTex) {
			glDeleteTextures(1, &m_ssrHiZTex);
			m_ssrHiZTex = 0;
		}
		m_ssrHiZWidth = 0;
		m_ssrHiZHeight = 0;
		m_ssrHiZLevels = 1;

		for (auto& [_, state] : m_taaStates) {
			for (int i = 0; i < 2; ++i) {
				if (state.historyTex[i]) {
					glDeleteTextures(1, &state.historyTex[i]);
					state.historyTex[i] = 0;
				}
				if (state.historyFBO[i]) {
					glDeleteFramebuffers(1, &state.historyFBO[i]);
					state.historyFBO[i] = 0;
				}
			}
			state.hasHistory = false;
		}
		m_taaStates.clear();

		for (auto& [_, state] : m_ssrTemporalStates) {
			for (int i = 0; i < 2; ++i) {
				if (state.historyColorTex[i]) {
					glDeleteTextures(1, &state.historyColorTex[i]);
					state.historyColorTex[i] = 0;
				}
				if (state.historyDepthTex[i]) {
					glDeleteTextures(1, &state.historyDepthTex[i]);
					state.historyDepthTex[i] = 0;
				}
				if (state.historyNormalTex[i]) {
					glDeleteTextures(1, &state.historyNormalTex[i]);
					state.historyNormalTex[i] = 0;
				}
				if (state.historyRoughnessTex[i]) {
					glDeleteTextures(1, &state.historyRoughnessTex[i]);
					state.historyRoughnessTex[i] = 0;
				}
			}
			state.hasHistory = false;
		}
		m_ssrTemporalStates.clear();
	}
}
