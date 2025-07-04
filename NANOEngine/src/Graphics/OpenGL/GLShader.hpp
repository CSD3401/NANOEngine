#ifndef GL_SHADER_HPP
#define GL_SHADER_HPP

#include "../Interfaces/IShader.hpp"
#include <unordered_map>

namespace NANOEngine::Graphics::OpenGL {

	class GLShader final : public IShader {
	public:
		GLShader(const std::string& filepath);
		~GLShader();

		void Bind() const override;
		void Unbind() const override;

		void SetUniformInt(const std::string& name, int value) override;
		void SetUniformFloat(const std::string& name, float value) override;
		void SetUniformVec3(const std::string& name, const Vec3& value) override;
		void SetUniformMat4(const std::string& name, const Mat4& matrix) override;

	private:
		uint32_t m_programID;
		std::unordered_map<std::string, int> m_uniformLocationCache;

		std::string LoadShaderSource(const std::string& path);
		std::unordered_map<unsigned int, std::string> Preprocess(const std::string& source);
		void Compile(const std::unordered_map<unsigned int, std::string>& shaderSources);

		int GetUniformLocation(const std::string& name);
	};

}

#endif // !GL_SHADER_HPP