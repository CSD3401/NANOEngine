#include "GraphicsManager.hpp"
#include "../../Math/Mat4.hpp"
#include "../OpenGL/GLCommandBuffer.hpp"
#include "../Interfaces/IShader.hpp"
#include "EditorCamera.hpp"
#include "Skybox.hpp"
#include <glad/glad.h>
#include "../../Core/Logger.hpp"
#include "../../Core/Profiler.hpp"
#include "../Interfaces/IFrameBuffer.hpp"
#include "../../ECS/Components/Light.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../OpenGL/GLPipeline.hpp"
#include "../OpenGL/GLTexture.hpp"
#include "../Core/Primitives.hpp"
#include "GizmosRenderer.hpp"
#include "UIRenderer.hpp"
#include "../OpenGL/GLStateCache.hpp"
#include "glfw/glfw3.h"
#include "Graphics/OpenGL/GLFrameBuffer.hpp"
#include "../../SceneManagement/Scene.hpp"
#include "Core/SpdLogger.hpp"
#include "InstanceData.hpp"
#include "../OpenGL/GLGeometryBuffer.hpp"
#include <GL/gl.h> // Add this include for OpenGL functions like glBegin, glEnd, etc.

// Experimental stuff
#include "ResourceManagement/ResourceManager.hpp"
#include "Input/InputManager.hpp"


namespace NE::Graphics {
    uint32_t GraphicsManager::s_ScreenWidth = 1920;
    uint32_t GraphicsManager::s_ScreenHeight = 1080;
    std::vector<ECS::Component::Light*> GraphicsManager::m_lights;
    int GraphicsManager::drawCount = 0;
    bool GraphicsManager::enableSorting = true;

    std::unique_ptr<ICommandBuffer> GraphicsManager::s_CommandBuffer;
    std::unique_ptr<Skybox> GraphicsManager::s_skybox;
    EditorCamera* GraphicsManager::s_EditorCamera;
	std::unique_ptr<IStateCache> GraphicsManager::s_StateCache;
	std::unique_ptr<DrawQueue> GraphicsManager::s_DrawQueue;
	std::unique_ptr<RenderViewManager> GraphicsManager::s_RenderViewManager;
    RenderViewHandle GraphicsManager::s_ActiveViewHandle;
    RenderViewHandle GraphicsManager::s_SceneViewHandle;
    RenderViewHandle GraphicsManager::s_FinalOutputViewHandle;
    RenderViewHandle GraphicsManager::s_GameViewHandle;

    std::vector<DebugLine> GraphicsManager::s_DebugLines;
    std::vector<DebugTriangle> GraphicsManager::s_DebugTriangles;
    std::vector<float> GraphicsManager::s_DebugVertexBuffer; // pre-allocated buffer to avoid reallocations
    int GraphicsManager::s_DebugViewLoc; // cached uniform locations (avoid glGetUniformLocation every frame)
    int GraphicsManager::s_DebugProjLoc;

    GLuint debugShaderProgram, debugVAO, debugVBO;

    RenderSettings GraphicsManager::renderSettings;

#pragma region EXPERIMENTAL
    // Experimental
    static GLuint s_QuadVAO = 0, s_QuadVBO = 0;
    static std::shared_ptr<NE::Graphics::OpenGL::GLShader> s_BrightPassShader;
    static uint32_t s_BrightPassTex = 0;
    static uint32_t s_BrightPassFBO = 0;
    static bool showBright = false;

    static const int BLOOM_LEVELS = 5;
    static GLuint s_BloomFBO[BLOOM_LEVELS];
    static GLuint s_BloomTex[BLOOM_LEVELS];
    static int s_BloomWidth[BLOOM_LEVELS];
    static int s_BloomHeight[BLOOM_LEVELS];

    // temp ping-pong target for blur & upsample
    static GLuint s_BloomTempFBO[BLOOM_LEVELS];
    static GLuint s_BloomTempTex[BLOOM_LEVELS];

    //static GLuint s_FinalFBO = 0;
    //static GLuint s_FinalColorTex = 0;

    static std::shared_ptr<NE::Graphics::OpenGL::GLShader> s_DownSampleShader;
    static std::shared_ptr<NE::Graphics::OpenGL::GLShader> s_BlurShader;
    static std::shared_ptr<NE::Graphics::OpenGL::GLShader> s_UpSampleShader;
    static std::shared_ptr<NE::Graphics::OpenGL::GLShader> s_CompositeShader;

    float GraphicsManager::s_brightThreshold = 1.f;
    float GraphicsManager::s_brightScale = 1.f;
    float GraphicsManager::s_brightSoftKnee = 0.2f;

    float GraphicsManager::s_upSampleIntensity = 0.8f;

    float GraphicsManager::s_bloomStrength = 0.1f;
    float GraphicsManager::s_tonemapExposure = 1.f;
    
    // Here for now i will shift it all to rendergraph next time
    void InitFullscreenQuadAndBrightpass()
    {
        if (s_QuadVAO == 0) {
            float quadVerts[] = {
                // pos      // uv
                -1.f, -1.f, 0.f, 0.f,
                 1.f, -1.f, 1.f, 0.f,
                -1.f,  1.f, 0.f, 1.f,
                 1.f,  1.f, 1.f, 1.f,
            };

            glGenVertexArrays(1, &s_QuadVAO);
            glGenBuffers(1, &s_QuadVBO);
            glBindVertexArray(s_QuadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, s_QuadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

            glBindVertexArray(0);
        }

        if (s_BrightPassFBO == 0) {
            glGenFramebuffers(1, &s_BrightPassFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, s_BrightPassFBO);

            glGenTextures(1, &s_BrightPassTex);
            glBindTexture(GL_TEXTURE_2D, s_BrightPassTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                1920, 1080, 0,
                GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, s_BrightPassTex, 0);

            GLenum att = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &att);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // load the bright-pass nanoshader
        if (!s_BrightPassShader) {
            s_BrightPassShader = Resource::ResourceManager::GetInstance().LoadResource<OpenGL::GLShader>("nebrightpass");
        }
    }
#pragma endregion

    void GraphicsManager::Init() {
        s_CommandBuffer = std::make_unique<OpenGL::GLCommandBuffer>();
        s_skybox = std::make_unique<Skybox>();
        s_StateCache = std::make_unique<OpenGL::GLStateCache>();
        s_DrawQueue = std::make_unique<DrawQueue>();
		s_RenderViewManager = std::make_unique<RenderViewManager>();

        s_SceneViewHandle = s_RenderViewManager->CreateHDR(1920, 1080, true);
        s_FinalOutputViewHandle = s_RenderViewManager->Create(1920, 1080, false);
        //s_GameViewHandle = s_RenderViewManager->Create(1920, 1080, false);

        NE::Graphics::OpenGL::GLGeometryBuffer::InitInstanceBuffer();

#pragma region EXPERIMENTAL
        InitFullscreenQuadAndBrightpass();

        int baseW = 1920;
        int baseH = 1080;

        int w = baseW;
        int h = baseH;

        for (int i = 0; i < BLOOM_LEVELS; ++i) {
            s_BloomWidth[i] = w;
            s_BloomHeight[i] = h;

            // main bloom level
            glGenFramebuffers(1, &s_BloomFBO[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, s_BloomFBO[i]);

            glGenTextures(1, &s_BloomTex[i]);
            glBindTexture(GL_TEXTURE_2D, s_BloomTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                w, h, 0,
                GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, s_BloomTex[i], 0);

            GLenum att = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &att);

            // temp blur/upsample target
            glGenFramebuffers(1, &s_BloomTempFBO[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, s_BloomTempFBO[i]);

            glGenTextures(1, &s_BloomTempTex[i]);
            glBindTexture(GL_TEXTURE_2D, s_BloomTempTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                w, h, 0,
                GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, s_BloomTempTex[i], 0);

            glDrawBuffers(1, &att);

            w = std::max(1, w / 2);
            h = std::max(1, h / 2);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (!s_DownSampleShader) {
            s_DownSampleShader = Resource::ResourceManager::GetInstance().LoadResource<OpenGL::GLShader>("nebloomdownsample");
        }
        if (!s_BlurShader) {
            s_BlurShader = Resource::ResourceManager::GetInstance().LoadResource<OpenGL::GLShader>("nebloomblur");
        }
        if (!s_UpSampleShader) {
            s_UpSampleShader = Resource::ResourceManager::GetInstance().LoadResource<OpenGL::GLShader>("nebloomupsample");
        }
        if (!s_CompositeShader) {
            s_CompositeShader = Resource::ResourceManager::GetInstance().LoadResource<OpenGL::GLShader>("nebloomcomposite");
        }


#pragma endregion

        //// Load Primitives
        //auto skinned = std::make_shared<OpenGL::GLShader>();
        //skinned->LoadFromFile("Library/Shaders/Skinned.nanoshader");
        //skinned->LoadFromFile("Library/Shaders/Skinned.nanoshader");
        //Asset::AssetManager::GetInstance().AddToMap<OpenGL::GLShader>(skinned, "Skinned");

        // initialize UI renderer
        //GLint viewport[4];
        //glGetIntegerv(GL_VIEWPORT, viewport);
        //s_ScreenWidth = static_cast<uint32_t>(viewport[2]);
        //s_ScreenHeight = static_cast<uint32_t>(viewport[3]);
        //
        //UIRenderer::Init(s_ScreenWidth, s_ScreenHeight, s_RenderViewManager.get());
    }

    void GraphicsManager::BeginFrame() 
    {
		s_StateCache->InvalidateAll();
        
        //s_CommandBuffer->BeginRenderPass();

        drawCount = 0;
    }

    void GraphicsManager::SubmitSkybox() 
    {
        if (s_skybox) s_skybox->Submit();
    }

    void GraphicsManager::DrawFrame()
    {
        NE_PROFILE_FUNCTION();

		int renderedViews = 0;
        for (auto& [handle, view] : s_RenderViewManager->GetAllRenderViews()) {
            if (!view.isActive) continue;
            if (view.isMain && view.order == 0) s_GameViewHandle = handle;
            
            s_RenderViewManager->Bind(handle);
            s_CommandBuffer->Begin();
            
			const Mat4& camProj = view.projection;
            const Mat4& camView = view.view;
			const Vec3& camPos = view.position;

            // Sort by RenderQueue -> Material -> Mesh
            if (enableSorting)
                s_DrawQueue->Sort(camPos);

            // Prepare instance data buffer and batching variables
            std::vector<InstanceData> instanceData;
            instanceData.reserve(32);
            std::shared_ptr<IGeometryBuffer> currentMesh;
            std::shared_ptr<Material> currentMaterial;

            auto flushBatch = [&]() {
                if (instanceData.empty() || !currentMesh || !currentMaterial || !currentMaterial->GetPipeline()->GetSpecification().shader)
                    return;

                NE::Graphics::OpenGL::GLGeometryBuffer::UpdateInstanceBuffer(
                    instanceData.data(),
                    instanceData.size() * sizeof(InstanceData)
                );

                // Bind pipeline & GL state
                auto pipeline = currentMaterial->GetPipeline();
                s_StateCache->Bind(pipeline);
                currentMaterial->Bind();
                currentMesh->Bind();

                // Upload transform matrix to shader
                auto shader = pipeline->GetSpecification().shader;
                shader->SetUniformMat4("u_View", camView);
                shader->SetUniformMat4("u_Projection", camProj);
                shader->SetUniformVec3("u_CameraPos", camPos);

                shader->SetUniformVec3("i_GlobalAmbientColor", renderSettings.ambientColour);
                shader->SetUniformFloat("i_GlobalAmbientIntensity", renderSettings.ambientIntensity);

                shader->SetUniformInt("i_FogEnabled", renderSettings.fogEnabled ? 1 : 0);
                shader->SetUniformVec3("i_FogColor", renderSettings.fogColour);
                shader->SetUniformInt("i_FogMode", static_cast<int>(renderSettings.fogMode));
                shader->SetUniformFloat("i_FogDensity", renderSettings.fogDensity);
                shader->SetUniformFloat("i_FogStart", renderSettings.fogStart);
                shader->SetUniformFloat("i_FogEnd", renderSettings.fogEnd);

                // Set lights
                shader->SetUniformInt("u_numLights", static_cast<int>(m_lights.size()));
                for (size_t i = 0; i < m_lights.size(); ++i) {
                    const auto* light = m_lights[i];
                    std::string base = "u_lights[" + std::to_string(i) + "]";
                    shader->SetUniformInt(base + ".type", light->type);
                    shader->SetUniformVec3(base + ".position", light->position);
                    shader->SetUniformVec3(base + ".direction", light->direction);
                    shader->SetUniformVec3(base + ".color", light->color);
                    shader->SetUniformFloat(base + ".intensity", light->intensity);
                    shader->SetUniformFloat(base + ".innerCutoff", light->innerCutoff);
                    shader->SetUniformFloat(base + ".outerCutoff", light->outerCutoff);
                    shader->SetUniformFloat(base + ".constant", light->constant);
                    shader->SetUniformFloat(base + ".linear", light->linear);
                    shader->SetUniformFloat(base + ".quadratic", light->quadratic);
                }

                // Draw mesh with instancing
                currentMesh->DrawInstanced(instanceData.size());
                currentMesh->Unbind();

                instanceData.clear();
                ++drawCount;

                };

            const auto& commands = s_DrawQueue->GetCommands();
            for (const auto& command : commands)
            {
                auto mesh = command.mesh;
                auto material = command.material;

                // Check compatibility with current batch
                bool compatible =
                    (mesh == currentMesh) &&
                    (material == currentMaterial);

                // Flush current batch if not compatible
                if (!compatible && !instanceData.empty()) {
                    flushBatch();
                }

                // Prepare to create new batch if not compatible
                if (!compatible) {
                    currentMesh = mesh;
                    currentMaterial = material;
                }

                NE::Graphics::InstanceData instance{};
                instance.model = command.transform;
                instance.idRGB = command.idRGB;

                instanceData.push_back(instance);
            }

            // Flush any remaining batch
            if (!instanceData.empty()) {
                flushBatch();
            }
            ++renderedViews;
            s_RenderViewManager->Unbind();
        }
        if (renderedViews > 0) {
            drawCount /= renderedViews;
        }

#pragma region EXPERIMENTAL
        uint32_t sceneTex = 0;
        auto fb = s_RenderViewManager->GetFramebuffer(s_SceneViewHandle);
        if (fb) {
            sceneTex = fb->GetColorAttachment();  // bypass GetSceneColorAttachment()
        }

        if (sceneTex != 0 && s_BrightPassShader) {
            glBindFramebuffer(GL_FRAMEBUFFER, s_BrightPassFBO);

            uint32_t w = fb ? fb->GetWidth() : 1920;
            uint32_t h = fb ? fb->GetHeight() : 1080;

            glViewport(0, 0, w, h);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            s_BrightPassShader->Bind();
            s_BrightPassShader->SetUniformInt("u_SceneTex", 0);
            s_BrightPassShader->SetUniformFloat("u_Threshold", s_brightThreshold);
            s_BrightPassShader->SetUniformFloat("u_Scale", s_brightScale);
            s_BrightPassShader->SetUniformFloat("u_SoftKnee", s_brightSoftKnee);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sceneTex);

            glBindVertexArray(s_QuadVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        GLuint srcTex = s_BrightPassTex;
        int srcW = fb ? fb->GetWidth() : 1920;
        int srcH = fb ? fb->GetHeight() : 1080;

        for (int level = 0; level < BLOOM_LEVELS; ++level) {
            int dstW = s_BloomWidth[level];
            int dstH = s_BloomHeight[level];

            glBindFramebuffer(GL_FRAMEBUFFER, s_BloomFBO[level]);
            glViewport(0, 0, dstW, dstH);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            s_DownSampleShader->Bind();
            s_DownSampleShader->SetUniformInt("u_Source", 0);
            s_DownSampleShader->SetUniformVec2("u_TexelSize", { 1.0f / srcW, 1.0f / srcH });

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, srcTex);

            glBindVertexArray(s_QuadVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            srcTex = s_BloomTex[level];
            srcW = dstW;
            srcH = dstH;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        for (int level = 0; level < BLOOM_LEVELS; ++level) {
            int w = s_BloomWidth[level];
            int h = s_BloomHeight[level];

            glBindFramebuffer(GL_FRAMEBUFFER, s_BloomTempFBO[level]);
            glViewport(0, 0, w, h);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            s_BlurShader->Bind();
            s_BlurShader->SetUniformInt("u_Source", 0);
            s_BlurShader->SetUniformVec2("u_TexelSize", { 1.0f / w, 1.0f / h });
            s_BlurShader->SetUniformVec2("u_Direction", { 1.0f, 0.0f });

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, s_BloomTex[level]);

            glBindVertexArray(s_QuadVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            glBindFramebuffer(GL_FRAMEBUFFER, s_BloomFBO[level]);
            glViewport(0, 0, w, h);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            s_BlurShader->Bind();
            s_BlurShader->SetUniformInt("u_Source", 0);
            s_BlurShader->SetUniformVec2("u_TexelSize", { 1.0f / w, 1.0f / h });
            s_BlurShader->SetUniformVec2("u_Direction", { 0.0f, 1.0f });

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, s_BloomTempTex[level]);

            glBindVertexArray(s_QuadVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        for (int level = BLOOM_LEVELS - 1; level > 0; --level) {
            int hi = level - 1;
            int w = s_BloomWidth[hi];
            int h = s_BloomHeight[hi];

            glBindFramebuffer(GL_FRAMEBUFFER, s_BloomFBO[hi]);
            glViewport(0, 0, w, h);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            s_UpSampleShader->Bind();
            s_UpSampleShader->SetUniformInt("u_LowRes", 0);
            s_UpSampleShader->SetUniformInt("u_HighRes", 1);
            s_UpSampleShader->SetUniformFloat("u_Intensity", 1.0f);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, s_BloomTex[level]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, s_BloomTex[hi]);

            glBindVertexArray(s_QuadVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        auto finalFBO = s_RenderViewManager->GetFramebuffer(s_FinalOutputViewHandle);
        if (fb && s_CompositeShader && finalFBO->GetColorAttachment() != 0) {
            uint32_t sceneTexHDR = fb->GetColorAttachment();
            GLuint bloomTex = s_BloomTex[0];

            uint32_t w = fb->GetWidth();
            uint32_t h = fb->GetHeight();

            glBindFramebuffer(GL_FRAMEBUFFER, finalFBO->GetColorAttachment());
            glViewport(0, 0, w, h);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            s_CompositeShader->Bind();
            s_CompositeShader->SetUniformInt("u_SceneHDR", 0);
            s_CompositeShader->SetUniformInt("u_Bloom", 1);
            s_CompositeShader->SetUniformFloat("u_BloomStrength", s_bloomStrength);
            s_CompositeShader->SetUniformFloat("u_Exposure", s_tonemapExposure);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sceneTexHDR);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, bloomTex);

            glBindVertexArray(s_QuadVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

#pragma endregion
    }

    void GraphicsManager::Submit(const DrawCommand& command) 
    {
		s_DrawQueue->Submit(command);
    }

    void GraphicsManager::EndFrame() 
    {
		s_RenderViewManager->Unbind();
        //s_CommandBuffer->EndRenderPass();
        //s_CommandBuffer->End();
    }

    void GraphicsManager::Clear() 
    {
        s_DrawQueue->Clear();
	}

    void GraphicsManager::Shutdown() 
    {
		s_RenderViewManager->Shutdown();
        s_skybox.reset();
        s_CommandBuffer.reset();

        if (debugVBO) {
            glDeleteBuffers(1, &debugVBO);
            debugVBO = 0;
        }

        if (debugVAO) {
            glDeleteVertexArrays(1, &debugVAO);
            debugVAO = 0;
        }

        if (debugShaderProgram) {
            glDeleteProgram(debugShaderProgram);
            debugShaderProgram = 0;
        }

        s_DebugLines.clear();
        s_DebugTriangles.clear();
        s_DebugVertexBuffer.clear();

        s_DebugLines.shrink_to_fit();
        s_DebugTriangles.shrink_to_fit();
        s_DebugVertexBuffer.shrink_to_fit();

        UIRenderer::Shutdown();
        NE::Graphics::GizmosRenderer::Cleanup();
        NE::Graphics::OpenGL::GLGeometryBuffer::ShutdownInstanceBuffer();
    }

    void GraphicsManager::SetEditorCamera(EditorCamera* cam) 
    {
        s_EditorCamera = cam;
    }

    EditorCamera* GraphicsManager::GetEditorCamera() 
    {
        return s_EditorCamera;
    }

    void GraphicsManager::UpdateEditorCameraData()
    {
        s_RenderViewManager->SetCameraData(
            s_SceneViewHandle, 
			s_EditorCamera->GetProjectionMatrix(),
			s_EditorCamera->GetViewMatrix(),
			s_EditorCamera->GetPosition(),
            false,
            0
        );
    }

    RenderViewHandle GraphicsManager::CreateRenderView(uint32_t width, uint32_t height, bool enablePicking) 
    {
        return s_RenderViewManager->Create(width, height, enablePicking);
	}

    void GraphicsManager::SetCameraData(RenderViewHandle viewHandle, const Math::Mat4& projection, const Math::Mat4& view, const Math::Vec3& position, bool isMain, uint16_t order)
    {
		s_RenderViewManager->SetCameraData(viewHandle, projection, view, position, isMain, order);
    }

    void GraphicsManager::EnableCamera(RenderViewHandle viewHandle)
    {
        s_RenderViewManager->EnableCamera(viewHandle);
	}

    void GraphicsManager::DisableCamera(RenderViewHandle viewHandle)
    {
        s_RenderViewManager->DisableCamera(viewHandle);
    }

    uint32_t GraphicsManager::ReadPixel(uint32_t x, uint32_t y) 
    {
        //SPD_DEBUG("Clicked on X: " << x << " Y: " << y);
		return s_RenderViewManager->GetFramebuffer(s_SceneViewHandle)->ReadPixel(x, y);
    }

    uint32_t GraphicsManager::GetSceneColorAttachment() 
    {
        if (InputManager::IsKeyDown('1')) return s_BrightPassTex;
        if (InputManager::IsKeyDown('2')) return s_BloomTex[0];
        if (InputManager::IsKeyDown('3')) return s_BloomTex[1];
        //if (InputManager::IsKeyDown('4')) return s_FinalColorTex;
        if (InputManager::IsKeyDown('4')) return s_RenderViewManager->GetFramebuffer(s_SceneViewHandle)->GetColorAttachment();

        auto framebuffer = s_RenderViewManager->GetFramebuffer(s_FinalOutputViewHandle);
        if (framebuffer) {
          return framebuffer->GetColorAttachment();
        }

		//auto framebuffer = s_RenderViewManager->GetFramebuffer(s_SceneViewHandle);
        //  if (framebuffer) {
        //  return framebuffer->GetColorAttachment();
		//}

		//return 0;
	}

    uint32_t GraphicsManager::GetGameColorAttachment()
    {
		auto framebuffer = s_RenderViewManager->GetFramebuffer(s_GameViewHandle);
        if (framebuffer) {
            return framebuffer->GetColorAttachment();
		}
		return 0;
    }

    IStateCache* GraphicsManager::GetStateCache() {
        return s_StateCache.get();
    }

    uint32_t GraphicsManager::GetScreenWidth() {
        return s_ScreenWidth;
    }

    uint32_t GraphicsManager::GetScreenHeight() {
        return s_ScreenHeight;
    }

    // Debug drawing test code
    const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;
    uniform mat4 u_View;
    uniform mat4 u_Projection;
    out vec3 color;
    void main() {
        gl_Position = u_Projection * u_View * vec4(aPos, 1.0);
        color = aColor;
    }
)";

    const char* fragmentShaderSource = R"(
    #version 330 core
    in vec3 color;
    out vec4 FragColor;
    void main() {
        FragColor = vec4(color, 1.0);
    }
)";

    void GraphicsManager::InitDebugPrimitives() {
        // Compile shaders
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        debugShaderProgram = glCreateProgram();
        glAttachShader(debugShaderProgram, vertexShader);
        glAttachShader(debugShaderProgram, fragmentShader);
        glLinkProgram(debugShaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // cache uniform locations ONCE at initialization
        GraphicsManager::s_DebugViewLoc = glGetUniformLocation(debugShaderProgram, "u_View");
        GraphicsManager::s_DebugProjLoc = glGetUniformLocation(debugShaderProgram, "u_Projection");

        glGenVertexArrays(1, &debugVAO);
        glGenBuffers(1, &debugVBO);

        glBindVertexArray(debugVAO);
        glBindBuffer(GL_ARRAY_BUFFER, debugVBO);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);

        // pre-allocate debug vertex buffer
        GraphicsManager::s_DebugVertexBuffer.reserve(INITIAL_DEBUG_BUFFER_SIZE);
    }

    void GraphicsManager::AddDebugLine(const Math::Vec3& from, const Math::Vec3& to, const Math::Vec3& color) 
    {
        s_DebugLines.push_back({ from, to, color });
    }

    void GraphicsManager::DrawDebugLines() 
    {
        if (s_DebugLines.empty()) return;

        // clear buffer but keep capacity (avoid reallocation)
        s_DebugVertexBuffer.clear();

        // reserve exact size needed (2 vertices * 6 floats per line)
        size_t requiredSize = s_DebugLines.size() * 12;
        if (s_DebugVertexBuffer.capacity() < requiredSize) {
            s_DebugVertexBuffer.reserve(requiredSize * 2); // Extra room for growth
        }

        // build vertex data
        for (const auto& line : s_DebugLines) {
            // vertex 1 (from)
            s_DebugVertexBuffer.push_back(line.from.x);
            s_DebugVertexBuffer.push_back(line.from.y);
            s_DebugVertexBuffer.push_back(line.from.z);
            s_DebugVertexBuffer.push_back(line.color.x);
            s_DebugVertexBuffer.push_back(line.color.y);
            s_DebugVertexBuffer.push_back(line.color.z);

            // vertex 2 (to)
            s_DebugVertexBuffer.push_back(line.to.x);
            s_DebugVertexBuffer.push_back(line.to.y);
            s_DebugVertexBuffer.push_back(line.to.z);
            s_DebugVertexBuffer.push_back(line.color.x);
            s_DebugVertexBuffer.push_back(line.color.y);
            s_DebugVertexBuffer.push_back(line.color.z);
        }

        // upload to GPU
        glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
        glBufferData(GL_ARRAY_BUFFER, s_DebugVertexBuffer.size() * sizeof(float), s_DebugVertexBuffer.data(), GL_STREAM_DRAW);

        // use shader
        glUseProgram(debugShaderProgram);

        // use cached uniform locations (no glGetUniformLocation call)
        glUniformMatrix4fv(s_DebugViewLoc, 1, GL_FALSE, s_EditorCamera->GetViewMatrix().Data());
        glUniformMatrix4fv(s_DebugProjLoc, 1, GL_FALSE, s_EditorCamera->GetProjectionMatrix().Data());

        // draw
        glBindVertexArray(debugVAO);
        glLineWidth(2.0f);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(s_DebugLines.size() * 2));

        // clear for next frame
        s_DebugLines.clear();

        s_StateCache->InvalidateAll(); // TEMP
    }

    void GraphicsManager::AddDebugTriangle(const Math::Vec3& v0, const Math::Vec3& v1, const Math::Vec3& v2, const Math::Vec3& color) {
        s_DebugTriangles.push_back({ v0, v1, v2, color });
    }

    void GraphicsManager::DrawDebugTriangles() {
        if (s_DebugTriangles.empty()) return;

        // clear buffer but keep capacity (avoid reallocation)
        s_DebugVertexBuffer.clear();

        // reserve exact size needed (2 vertices * 6 floats per line)
        size_t requiredSize = s_DebugTriangles.size() * 12;
        if (s_DebugVertexBuffer.capacity() < requiredSize) {
            s_DebugVertexBuffer.reserve(requiredSize * 2); // Extra room for growth
        }

        // build vertex data
        for (const auto& tri : s_DebugTriangles) {
            // Vertex 0
            s_DebugVertexBuffer.push_back(tri.v0.x);
            s_DebugVertexBuffer.push_back(tri.v0.y);
            s_DebugVertexBuffer.push_back(tri.v0.z);
            s_DebugVertexBuffer.push_back(tri.color.x);
            s_DebugVertexBuffer.push_back(tri.color.y);
            s_DebugVertexBuffer.push_back(tri.color.z);

            // Vertex 1
            s_DebugVertexBuffer.push_back(tri.v1.x);
            s_DebugVertexBuffer.push_back(tri.v1.y);
            s_DebugVertexBuffer.push_back(tri.v1.z);
            s_DebugVertexBuffer.push_back(tri.color.x);
            s_DebugVertexBuffer.push_back(tri.color.y);
            s_DebugVertexBuffer.push_back(tri.color.z);

            // Vertex 2
            s_DebugVertexBuffer.push_back(tri.v2.x);
            s_DebugVertexBuffer.push_back(tri.v2.y);
            s_DebugVertexBuffer.push_back(tri.v2.z);
            s_DebugVertexBuffer.push_back(tri.color.x);
            s_DebugVertexBuffer.push_back(tri.color.y);
            s_DebugVertexBuffer.push_back(tri.color.z);
        }

        // upload to GPU
        glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
        glBufferData(GL_ARRAY_BUFFER, s_DebugVertexBuffer.size() * sizeof(float), s_DebugVertexBuffer.data(), GL_STREAM_DRAW);

        // use shader
        glUseProgram(debugShaderProgram);

        // Use cached uniform locations (no glGetUniformLocation call!)
        glUniformMatrix4fv(s_DebugViewLoc, 1, GL_FALSE, s_EditorCamera->GetViewMatrix().Data());
        glUniformMatrix4fv(s_DebugProjLoc, 1, GL_FALSE, s_EditorCamera->GetProjectionMatrix().Data());

        // draw
        glBindVertexArray(debugVAO);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);

        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(s_DebugTriangles.size() * 3));

        s_DebugTriangles.clear();
    }

    void GraphicsManager::AddDebugLinesBatch(const std::vector<Math::Vec3>& positions, const Math::Vec3& color) {
        if (positions.size() < 2) return;

        // reserve space upfront to avoid reallocations
        const size_t lineCount = positions.size() / 2;
        s_DebugLines.reserve(s_DebugLines.size() + lineCount);

        // add lines in pairs
        for (size_t i = 0; i + 1 < positions.size(); i += 2) {
            s_DebugLines.push_back({ positions[i], positions[i + 1], color });
        }
    }

    void GraphicsManager::AddDebugTrianglesBatch(const std::vector<Math::Vec3>& positions, const Math::Vec3& color) {
        if (positions.size() < 3) return;

        // reserve space upfront to avoid reallocations
        const size_t triCount = positions.size() / 3;
        s_DebugTriangles.reserve(s_DebugTriangles.size() + triCount);

        // add triangles in groups of 3
        for (size_t i = 0; i + 2 < positions.size(); i += 3) {
            s_DebugTriangles.push_back({ positions[i], positions[i + 1], positions[i + 2], color });
        }
    }

    void GraphicsManager::DrawAllDebugGeometry() {
        if (s_DebugLines.empty() && s_DebugTriangles.empty()) return;

        s_DebugVertexBuffer.clear();

        // calculate exact size needed
        const size_t lineVertexCount = s_DebugLines.size() * 2;
        const size_t triVertexCount = s_DebugTriangles.size() * 3;
        const size_t totalFloats = (lineVertexCount * 6) + (triVertexCount * 6);

        // resize once (no clear/reserve dance needed)
        s_DebugVertexBuffer.resize(totalFloats);

        // get raw pointer to data - direct memory writes!
        float* ptr = s_DebugVertexBuffer.data();

        // add all line vertices
        for (const auto& line : s_DebugLines) {
            // vertex 1 (from)
            *ptr++ = line.from.x;
            *ptr++ = line.from.y;
            *ptr++ = line.from.z;
            *ptr++ = line.color.x;
            *ptr++ = line.color.y;
            *ptr++ = line.color.z;

            // vertex 2 (to)
            *ptr++ = line.to.x;
            *ptr++ = line.to.y;
            *ptr++ = line.to.z;
            *ptr++ = line.color.x;
            *ptr++ = line.color.y;
            *ptr++ = line.color.z;
        }

        // add all triangle vertices
        for (const auto& tri : s_DebugTriangles) {
            // vertex 0
            *ptr++ = tri.v0.x;
            *ptr++ = tri.v0.y;
            *ptr++ = tri.v0.z;
            *ptr++ = tri.color.x;
            *ptr++ = tri.color.y;
            *ptr++ = tri.color.z;

            // vertex 1
            *ptr++ = tri.v1.x;
            *ptr++ = tri.v1.y;
            *ptr++ = tri.v1.z;
            *ptr++ = tri.color.x;
            *ptr++ = tri.color.y;
            *ptr++ = tri.color.z;

            // vertex 2
            *ptr++ = tri.v2.x;
            *ptr++ = tri.v2.y;
            *ptr++ = tri.v2.z;
            *ptr++ = tri.color.x;
            *ptr++ = tri.color.y;
            *ptr++ = tri.color.z;
        }

        // single upload to GPU
        glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
        glBufferData(GL_ARRAY_BUFFER,
            totalFloats * sizeof(float),
            s_DebugVertexBuffer.data(),
            GL_STREAM_DRAW);

        // setup shader
        glUseProgram(debugShaderProgram);
        glUniformMatrix4fv(s_DebugViewLoc, 1, GL_FALSE, s_EditorCamera->GetViewMatrix().Data());
        glUniformMatrix4fv(s_DebugProjLoc, 1, GL_FALSE, s_EditorCamera->GetProjectionMatrix().Data());
        glBindVertexArray(debugVAO);
        glEnable(GL_DEPTH_TEST);

        // draw lines
        if (!s_DebugLines.empty()) {
            glLineWidth(2.0f);
            glDepthFunc(GL_LEQUAL);
            glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVertexCount));
        }

        // draw triangles
        if (!s_DebugTriangles.empty()) {
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
            glDrawArrays(GL_TRIANGLES,
                static_cast<GLsizei>(lineVertexCount),
                static_cast<GLsizei>(s_DebugTriangles.size() * 3));
        }

        // clear both buffers
        s_DebugLines.clear();
        s_DebugTriangles.clear();
    }

    void GraphicsManager::DrawUI() {
        UIRenderer::BeginFrame();
        UIRenderer::DrawUIFrame();
        //UIRenderer::DrawTestQuad();
        UIRenderer::EndFrame();
        UIRenderer::Draw3DUIFrame(s_SceneViewHandle);
        
        UIRenderer::Composite(s_SceneViewHandle);
        UIRenderer::ClearCommands();
    }
}
