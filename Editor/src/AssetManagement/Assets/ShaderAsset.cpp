#include "ShaderAsset.hpp"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <filesystem>
#include <glad/glad.h>
#include <Engine.hpp>

namespace Editor::Assets {
	namespace {
		std::string Trim(const std::string& str) {
			const char* whitespace = " \t\n\r";
			size_t start = str.find_first_not_of(whitespace);
			if (start == std::string::npos)
				return "";
			size_t end = str.find_last_not_of(whitespace);
			return str.substr(start, end - start + 1);
		}

		GLenum ShaderTypeFromString(std::string& type) {
			type = Trim(type);
			if (type == "vertex") return GL_VERTEX_SHADER;
			if (type == "fragment") return GL_FRAGMENT_SHADER;
			if (type == "compute") return GL_COMPUTE_SHADER;
		}

		std::string LoadShaderSource(const std::string& path) {
			std::ifstream file(path);
			std::stringstream ss;
			ss << file.rdbuf();
			return ss.str();
		}

		std::unordered_map<GLenum, std::string> Preprocess(const std::string& source) {
			std::unordered_map<GLenum, std::string> shaderSources;

			const std::string typeToken = "#type";
			size_t pos = source.find(typeToken);
			while (pos != std::string::npos) {
				size_t eol = source.find_first_of("\r\n", pos);
				std::string type = source.substr(pos + typeToken.length(), eol - pos - typeToken.length());
				size_t nextLinePos = source.find_first_not_of("\r\n", eol);
				size_t nextTypePos = source.find(typeToken, nextLinePos);
				shaderSources[ShaderTypeFromString(type)] = source.substr(nextLinePos, nextTypePos - nextLinePos);
				pos = nextTypePos;
			}

			return shaderSources;
		}
	}

	bool ShaderAsset::Cook(const std::string& sourcePath, const std::string& outPath) const {
		std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());

		const std::string source = LoadShaderSource(sourcePath);
		auto shaderStages = Preprocess(source);

		return NE::CookShader(sourcePath, outPath, shaderStages);
	}
	bool ShaderAsset::LoadImportSettings(const std::string& /*sourcePath*/)
	{
		return false;
	}
	bool ShaderAsset::SaveImportSettings(const std::string& /*sourcePath*/)
	{
		return false;
	}
}