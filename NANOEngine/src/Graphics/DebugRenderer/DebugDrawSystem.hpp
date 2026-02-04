#pragma once

#include "../../Math/Vec3.hpp"
#include <vector>

namespace NE {
	namespace Graphics {
		class EditorCamera;
		class IStateCache;
	}
}

namespace NE::Graphics {
	class DebugDrawSystem {
	public:
		static void Init();
		static void Shutdown();

		static void SetEditorCamera(EditorCamera* cam);
		static void SetStateCache(IStateCache* cache);

		static void AddLine(const Math::Vec3& from, const Math::Vec3& to, const Math::Vec3& color);
		static void AddTriangle(const Math::Vec3& v0, const Math::Vec3& v1, const Math::Vec3& v2, const Math::Vec3& color);
		static void AddLinesBatch(const std::vector<Math::Vec3>& positions, const Math::Vec3& color);
		static void AddTrianglesBatch(const std::vector<Math::Vec3>& positions, const Math::Vec3& color);

		static void DrawLines();
		static void DrawTriangles();
		static void DrawAll();

	private:
		struct DebugLine {
			Math::Vec3 from;
			Math::Vec3 to;
			Math::Vec3 color;
		};

		struct DebugTriangle {
			Math::Vec3 v0;
			Math::Vec3 v1;
			Math::Vec3 v2;
			Math::Vec3 color;
		};

		static constexpr size_t INITIAL_DEBUG_BUFFER_SIZE = 10000;

		static EditorCamera* s_EditorCamera;
		static IStateCache* s_StateCache;

		static std::vector<DebugLine> s_DebugLines;
		static std::vector<DebugTriangle> s_DebugTriangles;
		static std::vector<float> s_DebugVertexBuffer;
		static int s_DebugViewLoc;
		static int s_DebugProjLoc;
	};
}
