#include "DebugDrawSystem.hpp"

#include "../Interfaces/IStateCache.hpp"
#include "Graphics/Core/EditorCamera.hpp"

#include <glad/glad.h>
#include <GL/gl.h>

namespace NE::Graphics {
	EditorCamera* DebugDrawSystem::s_EditorCamera = nullptr;
	IStateCache* DebugDrawSystem::s_StateCache = nullptr;

	std::vector<DebugDrawSystem::DebugLine> DebugDrawSystem::s_DebugLines;
	std::vector<DebugDrawSystem::DebugTriangle> DebugDrawSystem::s_DebugTriangles;
	std::vector<float> DebugDrawSystem::s_DebugVertexBuffer;
	int DebugDrawSystem::s_DebugViewLoc = -1;
	int DebugDrawSystem::s_DebugProjLoc = -1;

	static GLuint s_DebugShaderProgram = 0;
	static GLuint s_DebugVAO = 0;
	static GLuint s_DebugVBO = 0;

	// Debug drawing shader sources
	static const char* s_VertexShaderSource = R"(
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

	static const char* s_FragmentShaderSource = R"(
    #version 330 core
    in vec3 color;
    out vec4 FragColor;
    void main() {
        FragColor = vec4(color, 1.0);
    }
)";

	void DebugDrawSystem::Init()
	{
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexShader, 1, &s_VertexShaderSource, nullptr);
		glCompileShader(vertexShader);

		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentShader, 1, &s_FragmentShaderSource, nullptr);
		glCompileShader(fragmentShader);

		s_DebugShaderProgram = glCreateProgram();
		glAttachShader(s_DebugShaderProgram, vertexShader);
		glAttachShader(s_DebugShaderProgram, fragmentShader);
		glLinkProgram(s_DebugShaderProgram);

		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		s_DebugViewLoc = glGetUniformLocation(s_DebugShaderProgram, "u_View");
		s_DebugProjLoc = glGetUniformLocation(s_DebugShaderProgram, "u_Projection");

		glGenVertexArrays(1, &s_DebugVAO);
		glGenBuffers(1, &s_DebugVBO);

		glBindVertexArray(s_DebugVAO);
		glBindBuffer(GL_ARRAY_BUFFER, s_DebugVBO);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);

		s_DebugVertexBuffer.reserve(INITIAL_DEBUG_BUFFER_SIZE);
	}

	void DebugDrawSystem::Shutdown()
	{
		if (s_DebugVBO) {
			glDeleteBuffers(1, &s_DebugVBO);
			s_DebugVBO = 0;
		}

		if (s_DebugVAO) {
			glDeleteVertexArrays(1, &s_DebugVAO);
			s_DebugVAO = 0;
		}

		if (s_DebugShaderProgram) {
			glDeleteProgram(s_DebugShaderProgram);
			s_DebugShaderProgram = 0;
		}

		s_DebugLines.clear();
		s_DebugTriangles.clear();
		s_DebugVertexBuffer.clear();

		s_DebugLines.shrink_to_fit();
		s_DebugTriangles.shrink_to_fit();
		s_DebugVertexBuffer.shrink_to_fit();
	}

	void DebugDrawSystem::SetEditorCamera(EditorCamera* cam)
	{
		s_EditorCamera = cam;
	}

	void DebugDrawSystem::SetStateCache(IStateCache* cache)
	{
		s_StateCache = cache;
	}

	void DebugDrawSystem::AddLine(const Math::Vec3& from, const Math::Vec3& to, const Math::Vec3& color)
	{
		s_DebugLines.push_back({ from, to, color });
	}

	void DebugDrawSystem::DrawLines()
	{
		if (s_DebugLines.empty()) return;

		s_DebugVertexBuffer.clear();

		size_t requiredSize = s_DebugLines.size() * 12;
		if (s_DebugVertexBuffer.capacity() < requiredSize) {
			s_DebugVertexBuffer.reserve(requiredSize * 2);
		}

		for (const auto& line : s_DebugLines) {
			s_DebugVertexBuffer.push_back(line.from.x);
			s_DebugVertexBuffer.push_back(line.from.y);
			s_DebugVertexBuffer.push_back(line.from.z);
			s_DebugVertexBuffer.push_back(line.color.x);
			s_DebugVertexBuffer.push_back(line.color.y);
			s_DebugVertexBuffer.push_back(line.color.z);

			s_DebugVertexBuffer.push_back(line.to.x);
			s_DebugVertexBuffer.push_back(line.to.y);
			s_DebugVertexBuffer.push_back(line.to.z);
			s_DebugVertexBuffer.push_back(line.color.x);
			s_DebugVertexBuffer.push_back(line.color.y);
			s_DebugVertexBuffer.push_back(line.color.z);
		}

		glBindBuffer(GL_ARRAY_BUFFER, s_DebugVBO);
		glBufferData(GL_ARRAY_BUFFER, s_DebugVertexBuffer.size() * sizeof(float), s_DebugVertexBuffer.data(), GL_STREAM_DRAW);

		glUseProgram(s_DebugShaderProgram);
		glUniformMatrix4fv(s_DebugViewLoc, 1, GL_FALSE, s_EditorCamera->GetViewMatrix().Data());
		glUniformMatrix4fv(s_DebugProjLoc, 1, GL_FALSE, s_EditorCamera->GetProjectionMatrix().Data());

		glBindVertexArray(s_DebugVAO);
		glLineWidth(2.0f);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);

		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(s_DebugLines.size() * 2));

		s_DebugLines.clear();

		if (s_StateCache) {
			s_StateCache->InvalidateAll();
		}
	}

	void DebugDrawSystem::AddTriangle(const Math::Vec3& v0, const Math::Vec3& v1, const Math::Vec3& v2, const Math::Vec3& color)
	{
		s_DebugTriangles.push_back({ v0, v1, v2, color });
	}

	void DebugDrawSystem::DrawTriangles()
	{
		if (s_DebugTriangles.empty()) return;

		s_DebugVertexBuffer.clear();

		size_t requiredSize = s_DebugTriangles.size() * 12;
		if (s_DebugVertexBuffer.capacity() < requiredSize) {
			s_DebugVertexBuffer.reserve(requiredSize * 2);
		}

		for (const auto& tri : s_DebugTriangles) {
			s_DebugVertexBuffer.push_back(tri.v0.x);
			s_DebugVertexBuffer.push_back(tri.v0.y);
			s_DebugVertexBuffer.push_back(tri.v0.z);
			s_DebugVertexBuffer.push_back(tri.color.x);
			s_DebugVertexBuffer.push_back(tri.color.y);
			s_DebugVertexBuffer.push_back(tri.color.z);

			s_DebugVertexBuffer.push_back(tri.v1.x);
			s_DebugVertexBuffer.push_back(tri.v1.y);
			s_DebugVertexBuffer.push_back(tri.v1.z);
			s_DebugVertexBuffer.push_back(tri.color.x);
			s_DebugVertexBuffer.push_back(tri.color.y);
			s_DebugVertexBuffer.push_back(tri.color.z);

			s_DebugVertexBuffer.push_back(tri.v2.x);
			s_DebugVertexBuffer.push_back(tri.v2.y);
			s_DebugVertexBuffer.push_back(tri.v2.z);
			s_DebugVertexBuffer.push_back(tri.color.x);
			s_DebugVertexBuffer.push_back(tri.color.y);
			s_DebugVertexBuffer.push_back(tri.color.z);
		}

		glBindBuffer(GL_ARRAY_BUFFER, s_DebugVBO);
		glBufferData(GL_ARRAY_BUFFER, s_DebugVertexBuffer.size() * sizeof(float), s_DebugVertexBuffer.data(), GL_STREAM_DRAW);

		glUseProgram(s_DebugShaderProgram);
		glUniformMatrix4fv(s_DebugViewLoc, 1, GL_FALSE, s_EditorCamera->GetViewMatrix().Data());
		glUniformMatrix4fv(s_DebugProjLoc, 1, GL_FALSE, s_EditorCamera->GetProjectionMatrix().Data());

		glBindVertexArray(s_DebugVAO);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);

		glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(s_DebugTriangles.size() * 3));

		s_DebugTriangles.clear();
	}

	void DebugDrawSystem::AddLinesBatch(const std::vector<Math::Vec3>& positions, const Math::Vec3& color)
	{
		if (positions.size() < 2) return;

		const size_t lineCount = positions.size() / 2;
		s_DebugLines.reserve(s_DebugLines.size() + lineCount);

		for (size_t i = 0; i + 1 < positions.size(); i += 2) {
			s_DebugLines.push_back({ positions[i], positions[i + 1], color });
		}
	}

	void DebugDrawSystem::AddTrianglesBatch(const std::vector<Math::Vec3>& positions, const Math::Vec3& color)
	{
		if (positions.size() < 3) return;

		const size_t triCount = positions.size() / 3;
		s_DebugTriangles.reserve(s_DebugTriangles.size() + triCount);

		for (size_t i = 0; i + 2 < positions.size(); i += 3) {
			s_DebugTriangles.push_back({ positions[i], positions[i + 1], positions[i + 2], color });
		}
	}

	void DebugDrawSystem::DrawAll()
	{
		if (s_DebugLines.empty() && s_DebugTriangles.empty()) return;

		s_DebugVertexBuffer.clear();

		const size_t lineVertexCount = s_DebugLines.size() * 2;
		const size_t triVertexCount = s_DebugTriangles.size() * 3;
		const size_t totalFloats = (lineVertexCount * 6) + (triVertexCount * 6);

		s_DebugVertexBuffer.resize(totalFloats);

		float* ptr = s_DebugVertexBuffer.data();

		for (const auto& line : s_DebugLines) {
			*ptr++ = line.from.x;
			*ptr++ = line.from.y;
			*ptr++ = line.from.z;
			*ptr++ = line.color.x;
			*ptr++ = line.color.y;
			*ptr++ = line.color.z;

			*ptr++ = line.to.x;
			*ptr++ = line.to.y;
			*ptr++ = line.to.z;
			*ptr++ = line.color.x;
			*ptr++ = line.color.y;
			*ptr++ = line.color.z;
		}

		for (const auto& tri : s_DebugTriangles) {
			*ptr++ = tri.v0.x;
			*ptr++ = tri.v0.y;
			*ptr++ = tri.v0.z;
			*ptr++ = tri.color.x;
			*ptr++ = tri.color.y;
			*ptr++ = tri.color.z;

			*ptr++ = tri.v1.x;
			*ptr++ = tri.v1.y;
			*ptr++ = tri.v1.z;
			*ptr++ = tri.color.x;
			*ptr++ = tri.color.y;
			*ptr++ = tri.color.z;

			*ptr++ = tri.v2.x;
			*ptr++ = tri.v2.y;
			*ptr++ = tri.v2.z;
			*ptr++ = tri.color.x;
			*ptr++ = tri.color.y;
			*ptr++ = tri.color.z;
		}

		glBindVertexArray(s_DebugVAO);
		glBindBuffer(GL_ARRAY_BUFFER, s_DebugVBO);
		glBufferData(GL_ARRAY_BUFFER,
			totalFloats * sizeof(float),
			s_DebugVertexBuffer.data(),
			GL_STREAM_DRAW);

		glUseProgram(s_DebugShaderProgram);
		glUniformMatrix4fv(s_DebugViewLoc, 1, GL_FALSE, s_EditorCamera->GetViewMatrix().Data());
		glUniformMatrix4fv(s_DebugProjLoc, 1, GL_FALSE, s_EditorCamera->GetProjectionMatrix().Data());
		glBindVertexArray(s_DebugVAO);
		glEnable(GL_DEPTH_TEST);

		if (!s_DebugLines.empty()) {
			glLineWidth(2.0f);
			glDepthFunc(GL_LEQUAL);
			glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVertexCount));
		}

		if (!s_DebugTriangles.empty()) {
			glDepthFunc(GL_LESS);
			glDepthMask(GL_TRUE);
			glDrawArrays(GL_TRIANGLES,
				static_cast<GLsizei>(lineVertexCount),
				static_cast<GLsizei>(s_DebugTriangles.size() * 3));
		}

		s_DebugLines.clear();
		s_DebugTriangles.clear();
	}
}
