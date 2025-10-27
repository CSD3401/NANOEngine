#ifndef GL_SHADER_HPP
#define GL_SHADER_HPP

#include "../Interfaces/IShader.hpp"
#include <unordered_map>

namespace NE::Graphics::OpenGL {

	struct UniformDesc { std::string name; unsigned int type; int size; };

	class GLShader final : public IShader {
	public:
		GLShader();
		GLShader(const std::string& filePath);
		~GLShader();

		void Bind() const override;
		void Unbind() const override;

		void SetUniformInt(const std::string& name, int value) override;
		void SetUniformFloat(const std::string& name, float value) override;
		void SetUniformVec3(const std::string& name, const Vec3& value) override;
		void SetUniformMat4(const std::string& name, const Mat4& matrix) override;

		void SetUniformHandle(const std::string& uName, uint64_t handle) override;
		void SetUniformHandlev(const std::string& uName, const uint64_t* handles, int count) override;

		bool LoadFromFile(const std::string& fileName) override;

		const uint32_t GetProgramID() const override { return m_programID; }
		const std::string_view GetUUID() const override { return uuid; } // Not implemented, return empty string

		std::vector<UniformDesc> EnumerateActiveUniforms() const;
		bool HasUniform(std::string_view name) const;
	private:
		uint32_t m_programID;
		std::unordered_map<std::string, int> m_uniformLocationCache;
		std::string LoadShaderSource(const std::string& path);
		std::unordered_map<unsigned int, std::string> Preprocess(const std::string& source);
		bool Compile(const std::unordered_map<unsigned int, std::string>& shaderSources);

		int GetUniformLocation(const std::string& name);
	};

}

#endif // !GL_SHADER_HPP