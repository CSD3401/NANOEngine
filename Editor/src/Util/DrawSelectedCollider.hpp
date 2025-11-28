// Editor/src/Util/DrawSelectedColliderOverlay.hpp
#pragma once
#include <vector>
#include <imgui/imgui.h>
#include <EditorInterface/ECSExports.hpp>
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Collider.hpp>
#include <Math/Vec3.hpp>
#include <Math/Mat4.hpp>

namespace EditorHelpers {

    static inline void MulPointToClip(const NE::Math::Mat4& M, const NE::Math::Vec3& p,
        float& x, float& y, float& z, float& w)
    {
        x = M.GetElement(0, 0) * p.x + M.GetElement(0, 1) * p.y + M.GetElement(0, 2) * p.z + M.GetElement(0, 3);
        y = M.GetElement(1, 0) * p.x + M.GetElement(1, 1) * p.y + M.GetElement(1, 2) * p.z + M.GetElement(1, 3);
        z = M.GetElement(2, 0) * p.x + M.GetElement(2, 1) * p.y + M.GetElement(2, 2) * p.z + M.GetElement(2, 3);
        w = M.GetElement(3, 0) * p.x + M.GetElement(3, 1) * p.y + M.GetElement(3, 2) * p.z + M.GetElement(3, 3);
    }

    // Project world -> screen inside the Scene panel rectangle
    static inline bool WorldToScreen(const NE::Math::Vec3& P,
        const NE::Math::Mat4& VP,
        const ImVec2& panelPos,
        const ImVec2& panelSize,
        ImVec2& outScreen)
    {
        float cx, cy, cz, cw;
        MulPointToClip(VP, P, cx, cy, cz, cw);
        if (cw <= 1e-5f) return false; // behind camera / invalid

        const float invW = 1.f / cw;
        const float ndcX = cx * invW;
        const float ndcY = cy * invW;

        // optional quick reject (comment out if you want to draw clipped edges)
        if (ndcX < -1.f || ndcX > 1.f || ndcY < -1.f || ndcY > 1.f)
            return false;

        const float u = ndcX * 0.5f + 0.5f;          // [0..1]
        const float v = ndcY * 0.5f + 0.5f;          // [0..1] (y up)
        outScreen.x = panelPos.x + u * panelSize.x;
        outScreen.y = panelPos.y + (1.f - v) * panelSize.y; 
        return true;
    }

    inline void DrawSelectedBoxColliderOverlay(uint32_t entity,
        const ImVec2& panelPos,
        const ImVec2& panelSize,
        const NE::Math::Mat4& view,
        const NE::Math::Mat4& proj,
        float thickness = 1.5f)
    {
        using namespace NE;

        if (!ECS::Query::HasTransform(entity) || !ECS::Query::HasCollider(entity)) return;
        const auto& tr = ECS::Query::GetEntityTransform(entity);
        const auto& col = ECS::Query::GetEntityCollider(entity);
        if (col.shapeType != ECS::Component::Collider::ShapeType::Box) return;

        const Math::Mat4 VP = proj * view;

        const Math::Vec3 h = col.halfExtents;
        Math::Vec3 L[8] = {
            {-h.x,-h.y,-h.z}, {+h.x,-h.y,-h.z}, {-h.x,+h.y,-h.z}, {+h.x,+h.y,-h.z},
            {-h.x,-h.y,+h.z}, {+h.x,-h.y,+h.z}, {-h.x,+h.y,+h.z}, {+h.x,+h.y,+h.z}
        };

        // world corners = tr.worldMatrix * localCorner
        Math::Vec3 W[8];
        for (int i = 0;i < 8;++i) {
            const auto& M = tr.worldMatrix;
            W[i].x = M.GetElement(0, 0) * L[i].x + M.GetElement(0, 1) * L[i].y + M.GetElement(0, 2) * L[i].z + M.GetElement(0, 3);
            W[i].y = M.GetElement(1, 0) * L[i].x + M.GetElement(1, 1) * L[i].y + M.GetElement(1, 2) * L[i].z + M.GetElement(1, 3);
            W[i].z = M.GetElement(2, 0) * L[i].x + M.GetElement(2, 1) * L[i].y + M.GetElement(2, 2) * L[i].z + M.GetElement(2, 3);
        }

        auto* dl = ImGui::GetWindowDrawList();
        const ImU32 col32 = IM_COL32(255, 230, 0, 255);

        auto edge = [&](int a, int b) {
            ImVec2 A, B;
            if (WorldToScreen(W[a], VP, panelPos, panelSize, A) &&
                WorldToScreen(W[b], VP, panelPos, panelSize, B)) {
                dl->AddLine(A, B, col32, thickness);
            }
            };

        // bottom
        edge(0, 1); edge(1, 3); edge(3, 2); edge(2, 0);
        // top
        edge(4, 5); edge(5, 7); edge(7, 6); edge(6, 4);
        // pillars
        edge(0, 4); edge(1, 5); edge(2, 6); edge(3, 7);
    }

} // namespace EditorHelpers
