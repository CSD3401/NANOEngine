#pragma once

#include <string>
#include <memory>

namespace NE::Graphics {
	namespace OpenGL {
		class GLShader;
	}
	std::shared_ptr<OpenGL::GLShader> LoadBuiltinShader(const std::string& sourcePath);
}
