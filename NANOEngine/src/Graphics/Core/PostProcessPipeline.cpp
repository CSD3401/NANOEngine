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
	void PostProcessPipeline::Init(RenderViewManager* rvm, uint32_t initialW, uint32_t initialH)
	{
		m_RVM = rvm;
		m_Width = initialW;
		m_Height = initialH;

		m_Pool = std::make_unique<TexturePool>();
		m_Graph = std::make_unique<RenderGraph>();
		m_Graph->SetTexturePool(m_Pool.get());

		InitFullscreenQuad();
		LoadShaders();
		InitBloomResources(m_Width, m_Height);
		InitSSAOResources(m_Width, m_Height);
	}

	void PostProcessPipeline::Shutdown()
	{
		DestroyResources(true);
		m_Graph.reset();
		m_Pool.reset();
		m_RVM = nullptr;
	}

	void PostProcessPipeline::Resize(uint32_t w, uint32_t h)
	{
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
		bool isSceneView)
	{
		if (!m_RVM || !m_Graph) return;

		m_Context.invProj = invProj;
		m_Context.isSceneView = isSceneView;

		SetupGraph(sourceView, destView, invProj, isSceneView);
		m_Graph->Execute();
	}

	void PostProcessPipeline::SetSettings(PostProcessingSettings* settings)
	{
		m_Settings = settings;
	}

	void PostProcessPipeline::InitFullscreenQuad()
	{
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

	void PostProcessPipeline::InitBloomResources(uint32_t w, uint32_t h)
	{
		if (w == 0 || h == 0) return;

		if (m_BrightPassFBO == 0) {
			glGenFramebuffers(1, &m_BrightPassFBO);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, m_BrightPassFBO);

		if (m_BrightPassTex == 0) {
			glGenTextures(1, &m_BrightPassTex);
		}
		glBindTexture(GL_TEXTURE_2D, m_BrightPassTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
			static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
			GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, m_BrightPassTex, 0);

		GLenum att = GL_COLOR_ATTACHMENT0;
		glDrawBuffers(1, &att);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		int levelW = static_cast<int>(w);
		int levelH = static_cast<int>(h);

		for (int i = 0; i < BLOOM_LEVELS; ++i) {
			m_BloomWidth[i] = levelW;
			m_BloomHeight[i] = levelH;

			if (m_BloomFBO[i] == 0) {
				glGenFramebuffers(1, &m_BloomFBO[i]);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[i]);

			if (m_BloomTex[i] == 0) {
				glGenTextures(1, &m_BloomTex[i]);
			}
			glBindTexture(GL_TEXTURE_2D, m_BloomTex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
				levelW, levelH, 0,
				GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, m_BloomTex[i], 0);
			glDrawBuffers(1, &att);

			if (m_BloomTempFBO[i] == 0) {
				glGenFramebuffers(1, &m_BloomTempFBO[i]);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, m_BloomTempFBO[i]);

			if (m_BloomTempTex[i] == 0) {
				glGenTextures(1, &m_BloomTempTex[i]);
			}
			glBindTexture(GL_TEXTURE_2D, m_BloomTempTex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
				levelW, levelH, 0,
				GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, m_BloomTempTex[i], 0);

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
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
			static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
			GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
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

	void PostProcessPipeline::LoadShaders()
	{
		if (!m_BrightPassShader) {
			m_BrightPassShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nebrightpass");
		}
		if (!m_DownSampleShader) {
			m_DownSampleShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nebloomdownsample");
		}
		if (!m_BlurShader) {
			m_BlurShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nebloomblur");
		}
		if (!m_UpSampleShader) {
			m_UpSampleShader = Resource::ResourceManager::GetInstance()
				.LoadResource<OpenGL::GLShader>("nebloomupsample");
		}
		if (!m_CompositeShader) {
			m_CompositeShader = Resource::ResourceManager::GetInstance()
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
		if (!m_RVM || !m_Graph) return;

		m_Graph->Clear();

		auto sourceFB = m_RVM->GetFramebuffer(sourceView);
		auto destFB = m_RVM->GetFramebuffer(destView);
		if (!sourceFB || !destFB) return;

		m_Context.sourceFB = sourceFB;
		m_Context.destFB = destFB;
		m_Context.invProj = invProj;
		m_Context.isSceneView = isSceneView;

		auto sceneColor = m_Graph->ImportTexture(sourceFB->GetColorAttachment(), "SceneHDR");
		auto sceneDepth = m_Graph->ImportTexture(sourceFB->GetDepthAttachment(), "SceneDepth");
		auto ssaoTex = m_Graph->ImportTexture(m_SSAOTex, "SSAO");
		auto brightTex = m_Graph->ImportTexture(m_BrightPassTex, "BrightPass");
		auto bloomTex = m_Graph->ImportTexture(m_BloomTex[0], "BloomResult");
		auto finalTex = m_Graph->ImportTexture(destFB->GetColorAttachment(), "FinalOutput");

		m_Graph->AddPass("SSAO")
			.Read(sceneDepth)
			.Write(ssaoTex)
			.Execute([this](const RenderGraphContext& ctx) {
				if (!m_Settings || !m_Settings->ssaoSettings.enabled || !m_SSAOShader) return;

				auto& pctx = m_Context;
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
				m_SSAOShader->SetUniformFloat("u_Radius", m_Settings->ssaoSettings.radius);
				m_SSAOShader->SetUniformFloat("u_Bias", m_Settings->ssaoSettings.bias);
				m_SSAOShader->SetUniformFloat("u_Intensity", m_Settings->ssaoSettings.intensity);
				m_SSAOShader->SetUniformFloat("u_Power", m_Settings->ssaoSettings.power);
				m_SSAOShader->SetUniformMat4("u_InvProj", pctx.invProj);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, pctx.sourceFB->GetDepthAttachment());

				glBindVertexArray(m_QuadVAO);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				glBindVertexArray(0);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
			});

		m_Graph->AddPass("Bloom: Bright Extract")
			.Read(sceneColor)
			.Write(brightTex)
			.Execute([this](const RenderGraphContext& ctx) {
				if (!m_Settings || !m_BrightPassShader) return;

				auto& pctx = m_Context;
				uint32_t w = pctx.sourceFB->GetWidth();
				uint32_t h = pctx.sourceFB->GetHeight();

				glBindFramebuffer(GL_FRAMEBUFFER, m_BrightPassFBO);
				glViewport(0, 0, w, h);
				glClearColor(0, 0, 0, 1);
				glClear(GL_COLOR_BUFFER_BIT);

				m_BrightPassShader->Bind();
				m_BrightPassShader->SetUniformInt("u_SceneTex", 0);
				m_BrightPassShader->SetUniformFloat("u_Threshold", m_Settings->bloomSettings.brightThreshold);
				m_BrightPassShader->SetUniformFloat("u_Scale", m_Settings->bloomSettings.brightScale);
				m_BrightPassShader->SetUniformFloat("u_SoftKnee", m_Settings->bloomSettings.softKnee);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, pctx.sourceFB->GetColorAttachment());

				glBindVertexArray(m_QuadVAO);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				glBindVertexArray(0);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			});

		m_Graph->AddPass("Bloom: Downsample")
			.Read(brightTex)
			.Write(bloomTex)
			.Execute([this](const RenderGraphContext& ctx) {
				if (!m_DownSampleShader) return;

				auto& pctx = m_Context;
				GLuint srcTex = m_BrightPassTex;
				int srcW = pctx.sourceFB->GetWidth();
				int srcH = pctx.sourceFB->GetHeight();

				for (int level = 0; level < BLOOM_LEVELS; ++level) {
					int dstW = m_BloomWidth[level];
					int dstH = m_BloomHeight[level];

					glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[level]);
					glViewport(0, 0, dstW, dstH);
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);

					m_DownSampleShader->Bind();
					m_DownSampleShader->SetUniformInt("u_Source", 0);
					m_DownSampleShader->SetUniformVec2("u_TexelSize", { 1.0f / srcW, 1.0f / srcH });

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, srcTex);

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

					srcTex = m_BloomTex[level];
					srcW = dstW;
					srcH = dstH;
				}
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			});

		m_Graph->AddPass("Bloom: Blur")
			.Read(bloomTex)
			.Write(bloomTex)
			.Execute([this](const RenderGraphContext& ctx) {
				if (!m_BlurShader) return;

				for (int level = 0; level < BLOOM_LEVELS; ++level) {
					int w = m_BloomWidth[level];
					int h = m_BloomHeight[level];

					glBindFramebuffer(GL_FRAMEBUFFER, m_BloomTempFBO[level]);
					glViewport(0, 0, w, h);
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);

					m_BlurShader->Bind();
					m_BlurShader->SetUniformInt("u_Source", 0);
					m_BlurShader->SetUniformVec2("u_TexelSize", { 1.0f / w, 1.0f / h });
					m_BlurShader->SetUniformVec2("u_Direction", { 1.0f, 0.0f });

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, m_BloomTex[level]);

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

					glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[level]);
					glViewport(0, 0, w, h);
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);

					m_BlurShader->SetUniformVec2("u_Direction", { 0.0f, 1.0f });

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, m_BloomTempTex[level]);

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				}
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			});

		m_Graph->AddPass("Bloom: Upsample")
			.Read(bloomTex)
			.Write(bloomTex)
			.Execute([this](const RenderGraphContext& ctx) {
				if (!m_Settings || !m_UpSampleShader) return;

				for (int level = BLOOM_LEVELS - 1; level > 0; --level) {
					int hi = level - 1;
					int w = m_BloomWidth[hi];
					int h = m_BloomHeight[hi];

					glBindFramebuffer(GL_FRAMEBUFFER, m_BloomFBO[hi]);
					glViewport(0, 0, w, h);
					glClearColor(0, 0, 0, 1);
					glClear(GL_COLOR_BUFFER_BIT);

					m_UpSampleShader->Bind();
					m_UpSampleShader->SetUniformInt("u_LowRes", 0);
					m_UpSampleShader->SetUniformInt("u_HighRes", 1);
					m_UpSampleShader->SetUniformFloat("u_Intensity", m_Settings->bloomSettings.bloomRadius);

					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, m_BloomTex[level]);
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, m_BloomTex[hi]);

					glBindVertexArray(m_QuadVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				}
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			});

		m_Graph->AddPass("Composite")
			.Read(sceneColor)
			.Read(ssaoTex)
			.Read(bloomTex)
			.Write(finalTex)
			.Execute([this](const RenderGraphContext& ctx) {
				if (!m_Settings || !m_CompositeShader) return;

				auto& pctx = m_Context;
				uint32_t w = pctx.sourceFB->GetWidth();
				uint32_t h = pctx.sourceFB->GetHeight();

				pctx.destFB->Bind();
				glViewport(0, 0, w, h);
				glClearColor(0, 0, 0, 1);
				glClear(GL_COLOR_BUFFER_BIT);

				glDisable(GL_DEPTH_TEST);
				glDepthMask(GL_FALSE);

				m_CompositeShader->Bind();
				m_CompositeShader->SetUniformInt("u_SceneHDR", 0);
				m_CompositeShader->SetUniformInt("u_Bloom", 1);
				m_CompositeShader->SetUniformInt("u_ToneMapType", static_cast<int>(m_Settings->bloomSettings.toneMapType));
				m_CompositeShader->SetUniformFloat("u_BloomStrength", m_Settings->bloomSettings.bloomIntensity);
				m_CompositeShader->SetUniformFloat("u_Exposure", m_Settings->bloomSettings.exposure);

				m_CompositeShader->SetUniformInt("u_SSAO", 2);
				m_CompositeShader->SetUniformInt("u_UseSSAO", m_Settings->ssaoSettings.enabled ? 1 : 0);
				m_CompositeShader->SetUniformFloat("u_AOIntensity", m_Settings->ssaoSettings.intensity);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, pctx.sourceFB->GetColorAttachment());

				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, m_BloomTex[0]);

				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, m_SSAOTex);

				glBindVertexArray(m_QuadVAO);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
				glBindVertexArray(0);

				glDepthMask(GL_TRUE);
				glEnable(GL_DEPTH_TEST);

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			});

		m_Graph->Compile();
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

		if (m_BrightPassTex) {
			glDeleteTextures(1, &m_BrightPassTex);
			m_BrightPassTex = 0;
		}
		if (m_BrightPassFBO) {
			glDeleteFramebuffers(1, &m_BrightPassFBO);
			m_BrightPassFBO = 0;
		}

		for (int i = 0; i < BLOOM_LEVELS; ++i) {
			if (m_BloomTex[i]) {
				glDeleteTextures(1, &m_BloomTex[i]);
				m_BloomTex[i] = 0;
			}
			if (m_BloomFBO[i]) {
				glDeleteFramebuffers(1, &m_BloomFBO[i]);
				m_BloomFBO[i] = 0;
			}
			if (m_BloomTempTex[i]) {
				glDeleteTextures(1, &m_BloomTempTex[i]);
				m_BloomTempTex[i] = 0;
			}
			if (m_BloomTempFBO[i]) {
				glDeleteFramebuffers(1, &m_BloomTempFBO[i]);
				m_BloomTempFBO[i] = 0;
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
