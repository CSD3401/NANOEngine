#pragma once
#include "IPanel.hpp"
#include <string>
#include <unordered_map>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace NE {
    namespace Animation {
        struct AnimatorController;
        struct State;
        struct Transition;
        struct Parameter;
    }
}

namespace Editor {

    class AnimatorGraphPanel : public IPanel {
    public:
        explicit AnimatorGraphPanel(const std::string& controllerPath = "")
            : m_ControllerPath(controllerPath) {
        }

        void SetControllerPath(const std::string& p) { m_ControllerPath = p; m_CachedPath.clear(); }
        void OnImGuiRender() override;

    private:
        // Canvas state
        ImVec2 m_Pan = ImVec2(0, 0);
        float  m_Zoom = 1.0f;

        // Layout: state index -> node position (canvas-local, un-zoomed)
        std::unordered_map<uint32_t, ImVec2> m_NodePos;

        // Selection
        int m_SelectedState = -1;
        struct LinkSel { int src = -1; int dst = -1; };
        LinkSel m_SelectedLink{};
        int m_PendingFrom = -1;

        // Controller cache (points to static in .cpp)
        std::string m_ControllerPath;
        std::string m_CachedPath;
        NE::Animation::AnimatorController* m_Ctrl = nullptr;

        // Helper API (implemented in .cpp)
        void   DrawCanvasBackground(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1);
        ImVec2 NodeSize(const NE::Animation::State& s) const;
        ImRect NodeRect(uint32_t idx, const NE::Animation::State& s, const ImVec2& origin);
        ImVec2 PinPosIn(const ImRect& r) const { return ImVec2(r.Min.x, (r.Min.y + r.Max.y) * 0.5f); }
        ImVec2 PinPosOut(const ImRect& r) const { return ImVec2(r.Max.x, (r.Min.y + r.Max.y) * 0.5f); }
        void   DrawNode(ImDrawList* dl, uint32_t idx, const NE::Animation::State& s, const ImVec2& origin);
        void   DrawLinks(ImDrawList* dl, const ImVec2& origin);
        bool   LinkHitTest(const ImVec2& p1, const ImVec2& p2, const ImVec2& mouse, float thickness, float& distOut);
        void   RightClickContext(const ImVec2& canvasMouse, const ImVec2& origin);
        void   DrawInspector();
    };

} // namespace Editor
