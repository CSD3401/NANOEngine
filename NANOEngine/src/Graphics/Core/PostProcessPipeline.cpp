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
		bool isSceneView
	) {
		if (!m_rvm || !m_graph) return;

		m_context.invProj = invProj;
		m_context.isSceneView = isSceneView;

		SetupGraph(sourceView, destView, invProj, isSceneView);
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
	}

	void PostProcessPipeline::SetupGraph(RenderViewHandle sourceView,
		RenderViewHandle destView,
		const Math::Mat4& invProj,
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

		auto sceneColor = m_graph->ImportTexture(sourceFB->GetColorAttachment(), "SceneHDR");
		auto sceneDepth = m_graph->ImportTexture(sourceFB->GetDepthAttachment(), "SceneDepth");
		auto ssaoTex = m_graph->ImportTexture(m_SSAOTex, "SSAO");
		auto brightTex = m_graph->ImportTexture(m_brightPassTex, "BrightPass");
		auto bloomTex = m_graph->ImportTexture(m_bloomTex[0], "BloomResult");
		auto finalTex = m_graph->ImportTexture(destFB->GetColorAttachment(), "FinalOutput");

		m_graph->AddPass("SSAO")
			.Read(sceneDepth)
			.Write(ssaoTex)
			.Execute([this](const RenderGraphContext& ctx) {
				if (!m_settings || !m_settings->ssaoSettings.enabled || !m_SSAOShader) return;

				auto& pctx = m_context;
				uint32_t w = pctx.sourceFB->GetWidth();
				uint32_t h = pctx.sourceFB->GetHeight();

				GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
				glDisable(GL_DEPTH_TEST);

				glBindFramebuffer(GL_FRAMEBUFFER, m_SSAOFBO);
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
				glBindTexture(GL_TEXTURE_2D, pctx.sourceFB->GetDepthAttachment());

				glBindVertexArray(m_QuadVAO);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				glBindVertexArray(0);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
			});

		m_graph->AddPass("Bloom: Bright Extract")
			.Read(sceneColor)
			.Write(brightTex)
			.Execute([this](const RenderGraphContext& ctx) {
				if (!m_settings || !m_brightPassShader) return;

				auto& pctx = m_context;
				uint32_t w = pctx.sourceFB->GetWidth();
				uint32_t h = pctx.sourceFB->GetHeight();

				glBindFramebuffer(GL_FRAMEBUFFER, m_brightPassFBO);
				glViewport(0, 0, w, h);
				glClearColor(0, 0, 0, 1);
				glClear(GL_COLOR_BUFFER_BIT);

				m_brightPassShader->Bind();
				m_brightPassShader->SetUniformInt("u_SceneTex", 0);
				m_brightPassShader->SetUniformFloat("u_Threshold", m_settings->bloomSettings.brightThreshold);
				m_brightPassShader->SetUniformFloat("u_Scale", m_settings->bloomSettings.brightScale);
				m_brightPassShader->SetUniformFloat("u_SoftKnee", m_settings->bloomSettings.softKnee);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, pctx.sourceFB->GetColorAttachment());

				glBindVertexArray(m_QuadVAO);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				glBindVertexArray(0);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			});

		m_graph->AddPass("Bloom: Downsample")
			.Read(brightTex)
			.Write(bloomTex)
			.Execute([this](const RenderGraphContext& ctx) {
				if (!m_downSampleShader) return;

				auto& pctx = m_context;
				GLuint srcTex = m_brightPassTex;
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

					// Avoid read/write hazards by ping-ponging to a temp target.
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

					std::swap(m_bloomTex[hi], m_bloomTempTex[hi]);
					std::swap(m_bloomFBO[hi], m_bloomTempFBO[hi]);
				}
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			});

		m_graph->AddPass("Composite")
			.Read(sceneColor)
			.Read(ssaoTex)
			.Read(bloomTex)
			.Write(finalTex)
			.Execute([this](const RenderGraphContext& ctx) {
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
				m_compositeShader->SetUniformFloat("u_BloomStrength", m_settings->bloomSettings.bloomIntensity);
				m_compositeShader->SetUniformFloat("u_Exposure", m_settings->bloomSettings.exposure);

				m_compositeShader->SetUniformInt("u_SSAO", 2);
				m_compositeShader->SetUniformInt("u_UseSSAO", m_settings->ssaoSettings.enabled ? 1 : 0);
				m_compositeShader->SetUniformFloat("u_AOIntensity", m_settings->ssaoSettings.intensity);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, pctx.sourceFB->GetColorAttachment());

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, m_bloomTex[0]);

				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, m_SSAOTex);

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
	}
}
