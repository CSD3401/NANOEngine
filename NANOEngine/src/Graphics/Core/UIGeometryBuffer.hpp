#pragma once
#include "../Interfaces/IGeometryBuffer.hpp"
#include "UIImageMeshGenerator.hpp"
#include <vector>
#include <cstdint>

namespace NE::Graphics {

    // Lightweight geometry buffer for UI rendering.
    // Owns its VAO/VBO/EBO and supports dynamic re-upload each frame.
    // Note: glad must be included before using this class (include in .cpp files).
    class UIGeometryBuffer final : public IGeometryBuffer {
    public:
        UIGeometryBuffer();
        ~UIGeometryBuffer() override;

        // Upload new vertex/index data (can be called every frame to reuse the buffer)
        void Upload(const std::vector<UIVertex2>& vertices, const std::vector<uint32_t>& indices);

        void Bind() const override;
        void Draw() const override;
        void Unbind() const override;
        void EnableInstanceLayout(int, int) override {} // UI shader has no per-instance attributes
        void DrawInstanced(size_t instanceCount) const override;

    private:
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;
        unsigned int m_ebo = 0;
        int m_indexCount = 0;
    };

} // namespace NE::Graphics
