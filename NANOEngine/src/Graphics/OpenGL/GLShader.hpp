#ifndef GL_SHADER_HPP
#define GL_SHADER_HPP

#include "../Interfaces/IShader.hpp"
#include <unordered_map>
#include "ResourceManagement/BinaryView.hpp"
#include "ResourceManagement/IResource.hpp"

namespace NE::Graphics::OpenGL {

	struct UniformDesc { std::string name; unsigned int type; int size; };

	class GLShader final : public IShader, public Resource::IResource {
	public:
		GLShader();
		~GLShader();

		void Bind() const override;
		void Unbind() const override;

		void SetUniformInt(const std::string& name, int value) override;
		void SetUniformFloat(const std::string& name, float value) override;
		void SetUniformVec3(const std::string& name, const Vec3& value) override;
		void SetUniformMat4(const std::string& name, const Mat4& matrix) override;

		void SetUniformHandle(const std::string& uName, uint64_t handle) override;
		void SetUniformHandlev(const std::string& uName, const uint64_t* handles, int count) override;

		const std::string_view GetUUID() const override {
			return std::string_view();
		};
		
		//const std::string_view GetUUID() const override { return uuid; } // Not implemented, return empty string

		bool Preload(NE::Resource::BinaryView blob) override;
		void Finalize() override;
		const uint32_t GetProgramID() const override { return m_programID; }

		std::vector<UniformDesc> EnumerateActiveUniforms() const;
		bool HasUniform(std::string_view name) const;

		void SetUniformMat4Array(const std::string& uName, const NE::Math::Mat4* data, int count) override;

		bool IsCompute() const override { return isCompute; }
		bool IsValid() const override { return m_programID != 0; }

	private:
		const uint8_t* progBlob = nullptr;
		size_t progSize = 0;
		uint32_t progFormat = 0;

		bool hasFallback = false;
		const char* vsSrc = nullptr;
		size_t vsLen = 0;
		const char* fsSrc = nullptr;
		size_t fsLen = 0;

		bool isCompute = false;
		const char* csSrc = nullptr;
		size_t csLen = 0;

		uint32_t m_programID;
		std::unordered_map<std::string, int> m_uniformLocationCache;

		int GetUniformLocation(const std::string& name);
	};

}

#endif // !GL_SHADER_HPP