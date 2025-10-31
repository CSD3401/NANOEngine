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
#include "../../AssetManager.hpp"
#include "../Core/Primitives.hpp"
#include "../OpenGL/GLStateCache.hpp"
#include "Graphics/OpenGL/GLFrameBuffer.hpp"
#include "../../SceneManagement/Scene.hpp"
#include <GL/gl.h> // Add this include for OpenGL functions like glBegin, glEnd, etc.


namespace NE::Graphics {
    void InitDebugLines();

    std::vector<ECS::Component::Light*> GraphicsManager::m_lights;
    int GraphicsManager::drawCount = 0;
    bool GraphicsManager::enableSorting = true;

	SceneManagement::RenderPass GraphicsManager::s_CurrentRenderPass = SceneManagement::RenderPass::SCENE;
    std::unique_ptr<ICommandBuffer> GraphicsManager::s_CommandBuffer;
    std::unique_ptr<Skybox> GraphicsManager::s_skybox;
    EditorCamera* GraphicsManager::s_EditorCamera;
	CameraData GraphicsManager::m_ActiveCamera;
	std::unique_ptr<IStateCache> GraphicsManager::s_StateCache;
	std::unique_ptr<DrawQueue> GraphicsManager::s_DrawQueue;
    std::vector<DebugLine> GraphicsManager::s_DebugLines;

    // Temp
	std::vector<DrawCommand> GraphicsManager::s_PickingCommands;
	std::shared_ptr<IFrameBuffer> GraphicsManager::s_ActiveFrameBuffer;
    std::shared_ptr<IFrameBuffer> GraphicsManager::s_SceneFrameBuffer;
    std::shared_ptr<IFrameBuffer> GraphicsManager::s_PickingFrameBuffer;
	std::shared_ptr<IFrameBuffer> GraphicsManager::s_GameFrameBuffer;

    void GraphicsManager::Init() 
    {
        s_CommandBuffer = std::make_unique<OpenGL::GLCommandBuffer>();
        s_skybox = std::make_unique<Skybox>();
        s_StateCache = std::make_unique<OpenGL::GLStateCache>();
        s_DrawQueue = std::make_unique<DrawQueue>();
        s_SceneFrameBuffer = std::make_shared<Graphics::OpenGL::GLFrameBuffer>(1920, 1080);
        s_PickingFrameBuffer = std::make_shared<Graphics::OpenGL::GLFrameBuffer>(1920, 1080);
		s_GameFrameBuffer = std::make_shared<Graphics::OpenGL::GLFrameBuffer>(1920, 1080);

        // Load Basic Shader
        //Asset::AssetManager::GetInstance().AddToMap<Graphics::IShader>(std::make_shared<OpenGL::GLShader>("Library/Shaders/Basic.nanoshader"), "Basic");
        //Asset::AssetManager::GetInstance().Load<Graphics::OpenGL::GLShader>("Library/Shaders/Basic.nanoshader", false);

        auto whiteTex = std::make_shared<OpenGL::GLTexture>();
        whiteTex->LoadFromFile("Library/Textures/white.jpg");
        Asset::AssetManager::GetInstance().AddToMap<OpenGL::GLTexture>(whiteTex, "WhiteTex");

        auto basic = std::make_shared<OpenGL::GLShader>();
        basic->LoadFromFile("Library/Shaders/Basic.nanoshader");
        Asset::AssetManager::GetInstance().AddToMap<OpenGL::GLShader>(basic, "Basic");

        auto unlit = std::make_shared<OpenGL::GLShader>();
        unlit->LoadFromFile("Library/Shaders/Unlit.nanoshader");
        Asset::AssetManager::GetInstance().AddToMap<OpenGL::GLShader>(unlit, "Unlit");

        auto litPBR = std::make_shared<OpenGL::GLShader>();
        litPBR->LoadFromFile("Library/Shaders/Lit_PBR.nanoshader");
        Asset::AssetManager::GetInstance().AddToMap<OpenGL::GLShader>(litPBR, "Lit_PBR");

        auto litBlinnPhong = std::make_shared<OpenGL::GLShader>();
        litBlinnPhong->LoadFromFile("Library/Shaders/Lit_BlinnPhong.nanoshader");
        Asset::AssetManager::GetInstance().AddToMap<OpenGL::GLShader>(litBlinnPhong, "Lit_BlinnPhong");

        // Load Primitives
        Asset::AssetManager::GetInstance().AddToMap<Graphics::Model>(CreateCube(), "Cube");
        Asset::AssetManager::GetInstance().AddToMap<Graphics::Model>(CreatePlane(), "Plane");
        Asset::AssetManager::GetInstance().AddToMap<Graphics::Model>(CreateCylinder(), "Cylinder");
        Asset::AssetManager::GetInstance().AddToMap<Graphics::Model>(CreateSphere(), "Sphere");
        Asset::AssetManager::GetInstance().AddToMap<Graphics::Model>(CreateCapsule(), "Capsule");

        // temp
        InitDebugLines();
    }

    void GraphicsManager::BeginFrame() 
    {
        drawCount = 0;
		s_StateCache->InvalidateAll();
        s_ActiveFrameBuffer->Bind();
        s_CommandBuffer->Begin();
        s_CommandBuffer->BeginRenderPass();
    }

    void GraphicsManager::DrawSkybox() 
    {
        if (s_skybox) s_skybox->Draw();
    }

    void GraphicsManager::DrawFrame() 
    {
        NE_PROFILE_FUNCTION();

        // TEMP?
		Mat4 camProj, camView;
		Vec3 camPos;
        switch (s_CurrentRenderPass) {
        case SceneManagement::RenderPass::SCENE:
        case SceneManagement::RenderPass::SCENE_PICKING:
			camProj = s_EditorCamera->GetProjectionMatrix();
			camView = s_EditorCamera->GetViewMatrix();
            camPos = s_EditorCamera->GetPosition();
			break;
        case SceneManagement::RenderPass::GAME:
            camProj = m_ActiveCamera.projection;
            camView = m_ActiveCamera.view;
            camPos = m_ActiveCamera.position;
			break;
            
        }

		if (enableSorting) s_DrawQueue->Sort(camPos);
		for (const auto& command : s_DrawQueue->GetCommands()) {
            // Bind the pipeline (shader program + GL state)
            //s_CommandBuffer->BindPipeline(command.material->GetPipeline());

            // Bind pipeline state and update the cache
            s_StateCache->Bind(command.material->GetPipeline());

            // Bind the vertex/index buffers
            command.mesh->Bind();

            // Bind material (textures, uniforms, etc.)
            command.material->Bind();

            // Upload transform matrix to shader
            auto shader = command.material->GetPipeline()->GetSpecification().shader;
            shader->SetUniformMat4("u_Model", command.transform);
            shader->SetUniformMat4("u_View", camView);
            shader->SetUniformMat4("u_Projection", camProj);
            shader->SetUniformMat4("u_NormalMatrix", command.transform.Inverse().Transpose());
            shader->SetUniformVec3("u_CameraPos", camPos);

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

            shader->SetUniformInt("u_ShadingModel", 1); // 0 = Phong, 1 = PBR

            // For object picking
            if (command.entity.has_value()) {
                float r = (float)(*command.entity & 0xFF) / 255.0f;
                float g = (float)((*command.entity >> 8) & 0xFF) / 255.0f;
                float b = (float)((*command.entity >> 16) & 0xFF) / 255.0f;
                shader->SetUniformVec3("u_ID", { r, g, b });
			}

            // Draw indexed
            //s_CommandBuffer->DrawIndexed(command.mesh->GetIndexCount());
            command.mesh->Draw();
            ++drawCount;

            glBindVertexArray(0);
		}
    }

    void GraphicsManager::Submit(const DrawCommand& command) 
    {
		s_DrawQueue->Submit(command);
    }

    void GraphicsManager::EndFrame() 
    {
        s_DrawQueue->Clear();
		s_ActiveFrameBuffer->Unbind();
        s_CommandBuffer->EndRenderPass();
        s_CommandBuffer->End();
    }

    void GraphicsManager::Shutdown() 
    {
		s_SceneFrameBuffer.reset();
		s_PickingFrameBuffer.reset();

        s_skybox.reset();
        s_CommandBuffer.reset();
    }

    void GraphicsManager::SetRenderPass(SceneManagement::RenderPass pass) 
    {
		s_CurrentRenderPass = pass;
        switch (pass) {
        case SceneManagement::RenderPass::SCENE:
            s_ActiveFrameBuffer = s_SceneFrameBuffer;
		    break;
        case SceneManagement::RenderPass::SCENE_PICKING:
			s_ActiveFrameBuffer = s_PickingFrameBuffer;
			break;
        case SceneManagement::RenderPass::GAME:
            s_ActiveFrameBuffer = s_GameFrameBuffer;
			break;
        default:
			break;
        }
    }

    void GraphicsManager::SubmitPicking(const DrawCommand& command) 
    {
        s_PickingCommands.push_back(command);
    }

    void GraphicsManager::UpdatePicking() 
    {
        for (const auto& cmd : s_PickingCommands) {
			s_DrawQueue->Submit(cmd);
		}
    }

    void GraphicsManager::SetEditorCamera(EditorCamera* cam) 
    {
        s_EditorCamera = cam;
    }

    EditorCamera* GraphicsManager::GetEditorCamera() 
    {
        return s_EditorCamera;
    }


    void GraphicsManager::SetActiveCamera(const Math::Mat4& projection, const Math::Mat4& view, const Math::Vec3& position, bool isMain) 
    {
		m_ActiveCamera.projection = projection;
		m_ActiveCamera.view = view;
		m_ActiveCamera.position = position;
		m_ActiveCamera.isMain = isMain;
	}

    uint32_t GraphicsManager::ReadPixel(uint32_t x, uint32_t y) 
    {
        s_PickingFrameBuffer->Bind();
        uint8_t data[4] = { 0, 0, 0, 0 };
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
        s_PickingFrameBuffer->Unbind();

        uint32_t id = data[0] | (data[1] << 8) | (data[2] << 16);
        return id;
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

    GLuint debugShaderProgram, debugVAO, debugVBO;

    void InitDebugLines() 
    {
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

        glGenVertexArrays(1, &debugVAO);
        glGenBuffers(1, &debugVBO);

        glBindVertexArray(debugVAO);
        glBindBuffer(GL_ARRAY_BUFFER, debugVBO);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void GraphicsManager::AddDebugLine(const Math::Vec3& from, const Math::Vec3& to, const Math::Vec3& color) 
    {
        s_DebugLines.push_back({ from, to, color });
    }

    void GraphicsManager::DrawDebugLines() 
    {
        if (s_DebugLines.empty()) return;

        std::vector<float> vertices;
        for (const auto& line : s_DebugLines) {
            vertices.push_back(line.from.x);
            vertices.push_back(line.from.y);
            vertices.push_back(line.from.z);
            vertices.push_back(line.color.x);
            vertices.push_back(line.color.y);
            vertices.push_back(line.color.z);

            vertices.push_back(line.to.x);
            vertices.push_back(line.to.y);
            vertices.push_back(line.to.z);
            vertices.push_back(line.color.x);
            vertices.push_back(line.color.y);
            vertices.push_back(line.color.z);
        }

        glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

        glUseProgram(debugShaderProgram);
        glBindVertexArray(debugVAO);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(s_DebugLines.size() * 2));

        s_DebugLines.clear();

        s_StateCache->InvalidateAll(); // TEMP
    }

}
