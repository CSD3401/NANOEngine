#include "PostProcessPipeline.hpp"

#include "PostProcessingSettings.hpp"
#include "../../Math/Mat4.hpp"
#include "RenderGraph.hpp"
#include "RenderViewManager.hpp"
#include "../Interfaces/IFrameBuffer.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../../ResourceManagement/ResourceManager.hpp"
#include "../../Core/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <glad/glad.h>
#include <GL/gl.h>

namespace NE::Graphics {
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
		if (!m_rvm || !m_graph) return;
		if (m_settings && m_settings->taaSettings.resetHistory) {
			for (auto& [_, state] : m_taaStates) {
				state.hasHistory = false;
			}
			m_settings->taaSettings.resetHistory = false;
		}

		m_context.invProj = invProj;
		m_context.isSceneView = isSceneView;
		m_context.currViewProjInv = (currProj * currView).Inverse();

		SetupGraph(sourceView, destView, invProj, currView, currProj, isSceneView);
		m_graph->Execute();
	}

	void PostProcessPipeline::SetSettings(PostProcessingSettings* settings) {
		m_settings = settings;
	}

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

		if (m_brightPassFBO == 0) {
			glGenFramebuffers(1, &m_brightPassFBO);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, m_brightPassFBO);

		if (m_brightPassTex == 0) {
			glGenTextures(1, &m_brightPassTex);
		}
		glBindTexture(GL_TEXTURE_2D, m_brightPassTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
			static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
			GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, m_brightPassTex, 0);

		GLenum att = GL_COLOR_ATTACHMENT0;
		glDrawBuffers(1, &att);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		int levelW = static_cast<int>(w);
		int levelH = static_cast<int>(h);

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
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
				levelW, levelH, 0,
				GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, m_bloomTex[i], 0);
			glDrawBuffers(1, &att);

			if (m_bloomTempFBO[i] == 0) {
				glGenFramebuffers(1, &m_bloomTempFBO[i]);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, m_bloomTempFBO[i]);

			if (m_bloomTempTex[i] == 0) {
				glGenTextures(1, &m_bloomTempTex[i]);
			}
			glBindTexture(GL_TEXTURE_2D, m_bloomTempTex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
				levelW, levelH, 0,
				GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, m_bloomTempTex[i], 0);

			glDrawBuffers(1, &att);

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
		if (!m_brightPassShader) {
			m_brightPassShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nebrightpass");
		}
		if (!m_downSampleShader) {
			m_downSampleShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nebloomdownsample");
		}
		if (!m_blurShader) {
			m_blurShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nebloomblur");
		}
		if (!m_upSampleShader) {
			m_upSampleShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nebloomupsample");
		}
		if (!m_compositeShader) {
			m_compositeShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nebloomcomposite");
		}
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

		const bool postEnabled = m_settings && m_settings->enabled;
		if (!postEnabled) {
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

			m_graph->Compile();
			return;
		}

		auto sceneColor = m_graph->ImportTexture(sourceFB->GetColorAttachment(), "SceneHDR");
		auto sceneDepth = m_graph->ImportTexture(sourceFB->GetDepthAttachment(), "SceneDepth");
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
				.Write(ssrRawTex)
				.Execute([this, ssrSceneInput, sceneDepth, sceneNormal, sceneRoughness, currView, currProj, ssrRawTex, ssrRawW, ssrRawH](const RenderGraphContext& ctx) {
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
					glClearColor(0, 0, 0, 0);
					glClear(GL_COLOR_BUFFER_BIT);

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
					m_ssrShader->SetUniformFloat("u_Stride", m_settings->ssrSettings.stride);
					m_ssrShader->SetUniformInt("u_BinarySearchSteps", m_settings->ssrSettings.binarySearchSteps);
					m_ssrShader->SetUniformFloat("u_RoughnessCutoff", m_settings->ssrSettings.roughnessCutoff);
					m_ssrShader->SetUniformFloat("u_EdgeFade", m_settings->ssrSettings.edgeFade);
					m_ssrShader->SetUniformFloat("u_MaxSceneMip", maxSceneMip);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(ssrSceneInput));
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

			m_graph->AddPass("SSR: Resolve")
				.Read(ssrRawTex)
				.Read(ssrSceneInput)
				.Read(sceneDepth)
				.Read(sceneNormal)
				.Read(sceneRoughness)
				.Write(ssrResolvedTex)
				.Execute([this, ssrRawTex, ssrSceneInput, sceneDepth, sceneNormal, sceneRoughness, currView, ssrResolvedTex, ssrRawW, ssrRawH, ssrResolveTapCount](const RenderGraphContext& ctx) {
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
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);

					m_ssrResolveShader->Bind();
					m_ssrResolveShader->SetUniformInt("u_SSRRaw", 0);
					m_ssrResolveShader->SetUniformInt("u_SceneColor", 1);
					m_ssrResolveShader->SetUniformInt("u_Depth", 2);
					m_ssrResolveShader->SetUniformInt("u_Normal", 3);
					m_ssrResolveShader->SetUniformInt("u_Roughness", 4);
					m_ssrResolveShader->SetUniformMat4("u_InvProj", pctx.invProj);
					m_ssrResolveShader->SetUniformMat4("u_View", currView);
					m_ssrResolveShader->SetUniformVec2("u_RawTexelSize", { 1.0f / static_cast<float>(ssrRawW), 1.0f / static_cast<float>(ssrRawH) });
					m_ssrResolveShader->SetUniformInt("u_ResolveTapCount", ssrResolveTapCount);
					m_ssrResolveShader->SetUniformFloat("u_Intensity", m_settings->ssrSettings.intensity);
					m_ssrResolveShader->SetUniformFloat("u_RoughnessCutoff", m_settings->ssrSettings.roughnessCutoff);
					m_ssrResolveShader->SetUniformFloat("u_FresnelPower", m_settings->ssrSettings.fresnelPower);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(ssrRawTex));
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(ssrSceneInput));
					glActiveTexture(GL_TEXTURE2);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneDepth));
					glActiveTexture(GL_TEXTURE3);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneNormal));
					glActiveTexture(GL_TEXTURE4);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(sceneRoughness));

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
					glBindVertexArray(0);

					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
				});

			sceneColor = ssrResolvedTex;
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
						glClearColor(0, 0, 0, 1);
						glClear(GL_COLOR_BUFFER_BIT);

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
			&& m_brightPassShader
			&& m_downSampleShader
			&& m_blurShader
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

		RenderGraphResource brightTex{};
		if (bloomEnabled) {
			TextureDesc brightDesc;
			brightDesc.width = sourceFB->GetWidth();
			brightDesc.height = sourceFB->GetHeight();
			brightDesc.format = TextureFormat::RGBA16F;
			brightDesc.name = "BrightPass";
			brightTex = m_graph->CreateTexture(brightDesc);
		}

		auto bloomTex = m_graph->ImportTexture(m_bloomTex[0], "BloomResult");
		auto finalTex = m_graph->ImportTexture(destFB->GetColorAttachment(), "FinalOutput");

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
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);

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
			const auto bloomSceneInput = sceneColor;
			m_graph->AddPass("Bloom: Bright Extract")
				.Read(bloomSceneInput)
				.Write(brightTex)
				.Execute([this, brightTex, bloomSceneInput](const RenderGraphContext& ctx) {
					if (!m_settings || !m_brightPassShader || !ctx.graph) return;

					auto& pctx = m_context;
					uint32_t w = pctx.sourceFB->GetWidth();
					uint32_t h = pctx.sourceFB->GetHeight();

					const uint32_t targetFbo = ctx.graph->GetFramebufferId(brightTex);
					if (targetFbo == 0) return;

					glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
					glViewport(0, 0, w, h);
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);

					m_brightPassShader->Bind();
					m_brightPassShader->SetUniformInt("u_SceneTex", 0);
					m_brightPassShader->SetUniformFloat("u_Threshold", m_settings->bloomSettings.brightThreshold);
					m_brightPassShader->SetUniformFloat("u_Scale", m_settings->bloomSettings.brightScale);
					m_brightPassShader->SetUniformFloat("u_SoftKnee", m_settings->bloomSettings.softKnee);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, ctx.GetTexture(bloomSceneInput));

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
					glBindVertexArray(0);

					glBindFramebuffer(GL_FRAMEBUFFER, 0);
				});

			m_graph->AddPass("Bloom: Downsample")
				.Read(brightTex)
				.Write(bloomTex)
				.Execute([this, brightTex](const RenderGraphContext& ctx) {
					if (!m_downSampleShader) return;

					auto& pctx = m_context;
					GLuint srcTex = ctx.GetTexture(brightTex);
					if (srcTex == 0) return;
					int srcW = pctx.sourceFB->GetWidth();
					int srcH = pctx.sourceFB->GetHeight();

					for (int level = 0; level < BLOOM_LEVELS; ++level) {
						int dstW = m_bloomWidth[level];
						int dstH = m_bloomHeight[level];

						glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[level]);
						glViewport(0, 0, dstW, dstH);
						glClearColor(0, 0, 0, 1);
						glClear(GL_COLOR_BUFFER_BIT);

						m_downSampleShader->Bind();
						m_downSampleShader->SetUniformInt("u_Source", 0);
						m_downSampleShader->SetUniformVec2("u_TexelSize", { 1.0f / srcW, 1.0f / srcH });

						glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_2D, srcTex);

						glBindVertexArray(m_QuadVAO);
						glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

						srcTex = m_bloomTex[level];
						srcW = dstW;
						srcH = dstH;
					}
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
				});

			m_graph->AddPass("Bloom: Blur")
				.Read(bloomTex)
				.Write(bloomTex)
				.Execute([this](const RenderGraphContext& ctx) {
					if (!m_blurShader) return;

					for (int level = 0; level < BLOOM_LEVELS; ++level) {
						int w = m_bloomWidth[level];
						int h = m_bloomHeight[level];

						glBindFramebuffer(GL_FRAMEBUFFER, m_bloomTempFBO[level]);
						glViewport(0, 0, w, h);
						glClearColor(0, 0, 0, 1);
						glClear(GL_COLOR_BUFFER_BIT);

						m_blurShader->Bind();
						m_blurShader->SetUniformInt("u_Source", 0);
						m_blurShader->SetUniformVec2("u_TexelSize", { 1.0f / w, 1.0f / h });
						m_blurShader->SetUniformVec2("u_Direction", { 1.0f, 0.0f });

						glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_2D, m_bloomTex[level]);

						glBindVertexArray(m_QuadVAO);
						glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

						glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFBO[level]);
						glViewport(0, 0, w, h);
						glClearColor(0, 0, 0, 1);
						glClear(GL_COLOR_BUFFER_BIT);

						m_blurShader->SetUniformVec2("u_Direction", { 0.0f, 1.0f });

						glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_2D, m_bloomTempTex[level]);

						glBindVertexArray(m_QuadVAO);
						glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
					}
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
				});

			m_graph->AddPass("Bloom: Upsample")
				.Read(bloomTex)
				.Write(bloomTex)
				.Execute([this](const RenderGraphContext& ctx) {
					if (!m_settings || !m_upSampleShader) return;

					for (int level = BLOOM_LEVELS - 1; level > 0; --level) {
						int hi = level - 1;
						int w = m_bloomWidth[hi];
						int h = m_bloomHeight[hi];

						glBindFramebuffer(GL_FRAMEBUFFER, m_bloomTempFBO[hi]);
						glViewport(0, 0, w, h);
						glClearColor(0, 0, 0, 1);
						glClear(GL_COLOR_BUFFER_BIT);

						m_upSampleShader->Bind();
						m_upSampleShader->SetUniformInt("u_LowRes", 0);
						m_upSampleShader->SetUniformInt("u_HighRes", 1);
						m_upSampleShader->SetUniformFloat("u_Intensity", m_settings->bloomSettings.bloomRadius);

						glActiveTexture(GL_TEXTURE0);
						glBindTexture(GL_TEXTURE_2D, m_bloomTex[level]);
						glActiveTexture(GL_TEXTURE1);
						glBindTexture(GL_TEXTURE_2D, m_bloomTex[hi]);

						glBindVertexArray(m_QuadVAO);
						glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

						glCopyImageSubData(
							m_bloomTempTex[hi], GL_TEXTURE_2D, 0, 0, 0, 0,
							m_bloomTex[hi], GL_TEXTURE_2D, 0, 0, 0, 0,
							w, h, 1
						);
					}
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
				});
		}

		m_graph->AddPass("Composite")
			.Read(sceneColor)
			.Read(ssaoTex)
			.Read(bloomTex)
			.Write(finalTex)
			.Execute([this, sceneColor, ssaoTex, bloomTex, bloomEnabled, ssaoEnabled](const RenderGraphContext& ctx) {
				if (!m_settings || !m_compositeShader) return;

				auto& pctx = m_context;
				uint32_t w = pctx.sourceFB->GetWidth();
				uint32_t h = pctx.sourceFB->GetHeight();

				pctx.destFB->Bind();
				glViewport(0, 0, w, h);
				glClearColor(0, 0, 0, 1);
				glClear(GL_COLOR_BUFFER_BIT);

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

		m_graph->Compile();
	}

	void PostProcessPipeline::DestroyResources(bool destroyQuad)
	{
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

		if (m_brightPassTex) {
			glDeleteTextures(1, &m_brightPassTex);
			m_brightPassTex = 0;
		}
		if (m_brightPassFBO) {
			glDeleteFramebuffers(1, &m_brightPassFBO);
			m_brightPassFBO = 0;
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
	}
}
