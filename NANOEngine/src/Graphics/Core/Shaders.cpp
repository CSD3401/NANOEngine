#include "Shaders.hpp"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <iostream>

#include <glad/glad.h>

#include "../OpenGL/GLShader.hpp"

namespace NE::Graphics {
	namespace {
		std::string Trim(const std::string& str) {
			const char* whitespace = " \t\n\r";
			size_t start = str.find_first_not_of(whitespace);
			if (start == std::string::npos)
				return "";
			size_t end = str.find_last_not_of(whitespace);
			return str.substr(start, end - start + 1);
		}

		const char* StageName(GLenum type) {
			switch (type) {
			case GL_VERTEX_SHADER:   return "vertex";
			case GL_FRAGMENT_SHADER: return "fragment";
			case GL_COMPUTE_SHADER:  return "compute";
			default:                 return "unknown";
			}
		}

		GLenum ShaderTypeFromString(std::string& type) {
			type = Trim(type);
			if (type == "vertex")   return GL_VERTEX_SHADER;
			if (type == "fragment") return GL_FRAGMENT_SHADER;
			if (type == "compute")  return GL_COMPUTE_SHADER;
			return 0; // unknown
		}

		std::string LoadShaderSource(const std::string& path) {
			std::ifstream file(path, std::ios::in | std::ios::binary);
			if (!file.is_open())
				return {};

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
				if (eol == std::string::npos)
					break;

				std::string type = source.substr(pos + typeToken.length(), eol - pos - typeToken.length());
				GLenum stage = ShaderTypeFromString(type);
				if (stage == 0) {
					// Unknown stage: skip safely
					size_t nextLinePos = source.find_first_not_of("\r\n", eol);
					pos = source.find(typeToken, nextLinePos);
					continue;
				}

				size_t nextLinePos = source.find_first_not_of("\r\n", eol);
				size_t nextTypePos = source.find(typeToken, nextLinePos);

				shaderSources[stage] = source.substr(
					nextLinePos,
					(nextTypePos == std::string::npos) ? std::string::npos : (nextTypePos - nextLinePos)
				);

				pos = nextTypePos;
			}

			return shaderSources;
		}

		bool CompileStage(GLenum stage, const std::string& src, GLuint& outShader, std::string& outLog) {
			outShader = glCreateShader(stage);
			const char* csrc = src.c_str();
			glShaderSource(outShader, 1, &csrc, nullptr);
			glCompileShader(outShader);

			GLint compiled = 0;
			glGetShaderiv(outShader, GL_COMPILE_STATUS, &compiled);
			if (compiled == GL_TRUE)
				return true;

			GLint len = 0;
			glGetShaderiv(outShader, GL_INFO_LOG_LENGTH, &len);
			outLog.clear();
			if (len > 1) {
				outLog.resize(static_cast<size_t>(len));
				glGetShaderInfoLog(outShader, len, nullptr, outLog.data());
			} else {
				outLog = "Unknown shader compile error.";
			}

			glDeleteShader(outShader);
			outShader = 0;
			return false;
		}

		bool LinkProgram(const std::vector<GLuint>& shaders, GLuint& outProgram, std::string& outLog) {
			outProgram = glCreateProgram();
			for (GLuint s : shaders)
				glAttachShader(outProgram, s);

			glLinkProgram(outProgram);

			GLint linked = 0;
			glGetProgramiv(outProgram, GL_LINK_STATUS, &linked);
			if (linked == GL_TRUE)
				return true;

			GLint len = 0;
			glGetProgramiv(outProgram, GL_INFO_LOG_LENGTH, &len);
			outLog.clear();
			if (len > 1) {
				outLog.resize(static_cast<size_t>(len));
				glGetProgramInfoLog(outProgram, len, nullptr, outLog.data());
			} else {
				outLog = "Unknown program link error.";
			}

			glDeleteProgram(outProgram);
			outProgram = 0;
			return false;
		}
	}

	std::shared_ptr<OpenGL::GLShader> LoadBuiltinShader(const std::string& sourcePath) {
		const std::string source = LoadShaderSource(sourcePath);
		if (source.empty()) {
			std::cerr << "Failed to read shader file: " << sourcePath << "\n";
			return nullptr;
		}

		auto sources = Preprocess(source);
		if (sources.empty()) {
			std::cerr << "No shader stages found in: " << sourcePath << "\n";
			return nullptr;
		}

		std::vector<GLuint> compiledStages;
		compiledStages.reserve(sources.size());

		// Compile stages
		for (auto& [stage, src] : sources) {
			GLuint shader = 0;
			std::string log;

			if (!CompileStage(stage, src, shader, log)) {
				std::cerr << "Shader compile failed (" << StageName(stage) << ") " << sourcePath << "\n"
					<< log << "\n";

				for (GLuint s : compiledStages) glDeleteShader(s);
				return nullptr;
			}

			compiledStages.push_back(shader);
		}

		// Link
		GLuint program = 0;
		{
			std::string linkLog;
			if (!LinkProgram(compiledStages, program, linkLog)) {
				std::cerr << "Program link failed " << sourcePath << "\n"
					<< linkLog << "\n";

				for (GLuint s : compiledStages) glDeleteShader(s);
				return nullptr;
			}
		}

		// Shaders can be deleted after a successful link
		for (GLuint s : compiledStages) {
			glDetachShader(program, s);
			glDeleteShader(s);
		}

		// GLShader should own program and delete it in its destructor.
		return std::make_shared<OpenGL::GLShader>(program);
	}
}
