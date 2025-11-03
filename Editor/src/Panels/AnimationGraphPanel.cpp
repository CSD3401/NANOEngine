#include "../Panels/AnimationGraphPanel.hpp"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <filesystem>
#include <algorithm>
#include <limits>
#include <cmath>

#include <Animation/AnimatorController.hpp>
#include <Animation/AnimatorControllerIO.hpp>

using namespace NE::Animation;

namespace {
    struct CtrlCache {
        AnimatorController ctrl;
        std::string path;
    };
    static CtrlCache G;
}

// small ImVec2 helpers (avoid operator overloads / name clashes)
static inline ImVec2 VAdd(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x + b.x, a.y + b.y); }
static inline ImVec2 VSub(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x - b.x, a.y - b.y); }
static inline ImVec2 VMul(const ImVec2& a, float s) { return ImVec2(a.x * s, a.y * s); }

namespace Editor {

    static ImVec2 VLerp(const ImVec2& a, const ImVec2& b, float t) { return VAdd(a, VMul(VSub(b, a), t)); }

    void AnimatorGraphPanel::OnImGuiRender() {
        ImGui::Begin("Animator Controller (Graph)");

        // Controller path field
        {
            char buf[512];
            std::snprintf(buf, sizeof(buf), "%s", m_ControllerPath.c_str());
            bool edited = ImGui::InputText("Controller", buf, IM_ARRAYSIZE(buf));
            bool commit = edited && ImGui::IsItemDeactivatedAfterEdit();
            ImGui::SameLine();
            if (ImGui::Button("Reload")) commit = true;
            ImGui::SameLine();
            if (ImGui::Button("Save") && G.path == m_ControllerPath) {
                SaveAnimatorController(G.ctrl, G.path);
            }
            if (commit) { m_ControllerPath = buf; m_CachedPath.clear(); }
        }

        // Load on path change
        if (m_CachedPath != m_ControllerPath) {
            m_Ctrl = nullptr;
            if (!m_ControllerPath.empty() && std::filesystem::is_regular_file(m_ControllerPath)) {
                if (LoadAnimatorController(G.ctrl, m_ControllerPath)) {
                    G.path = m_ControllerPath;
                    m_Ctrl = &G.ctrl;
                    m_CachedPath = m_ControllerPath;

                    // default layout
                    m_NodePos.clear();
                    for (uint32_t i = 0; i < G.ctrl.states.size(); ++i)
                        m_NodePos[i] = ImVec2(100.0f + (i % 5) * 240.0f, 100.0f + (i / 5) * 180.0f);
                    m_SelectedState = -1; m_SelectedLink = {}; m_PendingFrom = -1;
                }
            }
        }
        else {
            m_Ctrl = (G.path == m_ControllerPath) ? &G.ctrl : nullptr;
        }

        if (!m_Ctrl) {
            ImGui::TextDisabled("Load a valid controller JSON to edit.");
            ImGui::End();
            return;
        }

        // Canvas
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
        ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
        if (canvas_sz.x < 100) canvas_sz.x = 100;
        if (canvas_sz.y < 100) canvas_sz.y = 100;
        ImVec2 canvas_p1 = VAdd(canvas_p0, canvas_sz);

        // Background
        dl->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(30, 30, 35, 255));
        DrawCanvasBackground(dl, canvas_p0, canvas_p1);

        // Interaction surface
        ImGui::InvisibleButton("canvas", canvas_sz,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
        const bool canvasHovered = ImGui::IsItemHovered();
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const ImVec2 localMouse = VSub(mouse, canvas_p0);

        // Pan
        if (canvasHovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
            (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && m_SelectedState < 0 && m_PendingFrom < 0))) {
            m_Pan.x += ImGui::GetIO().MouseDelta.x;
            m_Pan.y += ImGui::GetIO().MouseDelta.y;
        }

        // Zoom
        if (canvasHovered && ImGui::GetIO().MouseWheel != 0.0f) {
            float before = m_Zoom;
            m_Zoom = std::clamp(m_Zoom + ImGui::GetIO().MouseWheel * 0.1f, 0.3f, 2.0f);
            ImVec2 o = m_Pan;
            o.x -= (localMouse.x - o.x) * (m_Zoom / before - 1.0f);
            o.y -= (localMouse.y - o.y) * (m_Zoom / before - 1.0f);
            m_Pan = o;
        }

        // Origin
        const ImVec2 origin = VAdd(canvas_p0, m_Pan);

        // Links then nodes
        DrawLinks(dl, origin);
        for (uint32_t i = 0; i < m_Ctrl->states.size(); ++i)
            DrawNode(dl, i, m_Ctrl->states[i], origin);

        // Context menu
        if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            RightClickContext(mouse, origin);

        // Inspector
        ImGui::SameLine();
        ImGui::BeginChild("Inspector", ImVec2(300, 0), true);
        DrawInspector();
        ImGui::EndChild();

        ImGui::End();
    }

    void AnimatorGraphPanel::DrawCanvasBackground(ImDrawList* dl, const ImVec2& p0, const ImVec2& p1) {
        const float grid = 32.0f * m_Zoom;
        ImU32 col = IM_COL32(50, 50, 60, 255);
        for (float x = std::fmod(m_Pan.x, grid) + p0.x; x < p1.x; x += grid)
            dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), col);
        for (float y = std::fmod(m_Pan.y, grid) + p0.y; y < p1.y; y += grid)
            dl->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), col);
    }

    ImVec2 AnimatorGraphPanel::NodeSize(const State& s) const {
        float w = 160.0f + std::max(0.0f, (float)s.name.size() * 2.5f);
        float h = 64.0f;
        return ImVec2(w * m_Zoom, h * m_Zoom);
    }

    ImRect AnimatorGraphPanel::NodeRect(uint32_t idx, const State& s, const ImVec2& origin) {
        ImVec2 pos = m_NodePos.count(idx) ? m_NodePos.at(idx) : ImVec2(100, 100);
        ImVec2 sz = NodeSize(s);
        ImVec2 tl = VAdd(origin, VMul(pos, m_Zoom));
        ImVec2 br = VAdd(tl, sz);
        return ImRect(tl, br);
    }

    void AnimatorGraphPanel::DrawNode(ImDrawList* dl, uint32_t idx, const State& s, const ImVec2& origin) {
        ImRect r = NodeRect(idx, s, origin);

        ImGui::SetCursorScreenPos(r.Min);
        ImGui::InvisibleButton((std::string("node##") + std::to_string(idx)).c_str(), r.GetSize());
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();

        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            m_NodePos[idx].x += ImGui::GetIO().MouseDelta.x / m_Zoom;
            m_NodePos[idx].y += ImGui::GetIO().MouseDelta.y / m_Zoom;
        }
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_SelectedState = (int)idx;
            m_SelectedLink = {};
        }

        ImU32 body = (m_SelectedState == (int)idx) ? IM_COL32(80, 110, 180, 255) : IM_COL32(68, 77, 100, 255);
        ImU32 border = IM_COL32(20, 20, 25, 255);
        dl->AddRectFilled(r.Min, r.Max, body, 6.0f);
        dl->AddRect(r.Min, r.Max, border, 6.0f, 0, 1.5f);

        ImVec2 titlePos = ImVec2(r.Min.x + 10, r.Min.y + 8);
        dl->AddText(titlePos, IM_COL32_WHITE, s.name.empty() ? "(unnamed)" : s.name.c_str());

        ImVec2 pinIn = PinPosIn(r);
        ImVec2 pinOut = PinPosOut(r);
        dl->AddCircleFilled(pinIn, 6.0f, IM_COL32(220, 220, 220, 255));
        dl->AddCircleFilled(pinOut, 6.0f, IM_COL32(220, 220, 220, 255));

        ImRect outHit(ImVec2(pinOut.x - 8, pinOut.y - 8), ImVec2(pinOut.x + 8, pinOut.y + 8));
        if (ImGui::IsMouseHoveringRect(outHit.Min, outHit.Max) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_PendingFrom = (int)idx;
            m_SelectedLink = {};
        }

        if (m_PendingFrom >= 0 && (int)idx != m_PendingFrom) {
            ImRect inHit(ImVec2(pinIn.x - 8, pinIn.y - 8), ImVec2(pinIn.x + 8, pinIn.y + 8));
            if (ImGui::IsMouseHoveringRect(inHit.Min, inHit.Max) && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                State& src = m_Ctrl->states[(uint32_t)m_PendingFrom];
                auto dupe = std::find_if(src.transitions.begin(), src.transitions.end(),
                    [&](const Transition& t) { return t.toState == idx; });
                if (dupe == src.transitions.end()) {
                    Transition t; t.toState = idx; t.duration = 0.2f; t.hasExitTime = false;
                    src.transitions.push_back(t);
                }
                m_SelectedLink = { m_PendingFrom, (int)idx };
                m_PendingFrom = -1;
            }
        }

        if (m_PendingFrom == (int)idx) {
            ImVec2 p1 = pinOut;
            ImVec2 p2 = ImGui::GetIO().MousePos;
            ImVec2 c1 = VAdd(p1, ImVec2(50.0f, 0));
            ImVec2 c2 = VSub(p2, ImVec2(50.0f, 0));
            dl->AddBezierCubic(p1, c1, c2, p2, IM_COL32(240, 240, 80, 255), 2.0f);
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                m_PendingFrom = -1;
        }
    }

    void AnimatorGraphPanel::DrawLinks(ImDrawList* dl, const ImVec2& origin) {
        const float thickness = 2.0f;
        const ImU32 col = IM_COL32(200, 200, 200, 255);
        const ImU32 colSel = IM_COL32(255, 230, 120, 255);

        float bestDist = 6.0f; int bestSrc = -1, bestDst = -1;

        for (uint32_t i = 0; i < m_Ctrl->states.size(); ++i) {
            const State& s = m_Ctrl->states[i];
            ImRect rS = NodeRect(i, s, origin);
            ImVec2 p1 = PinPosOut(rS);

            for (const auto& t : s.transitions) {
                if (t.toState >= m_Ctrl->states.size()) continue;
                const State& d = m_Ctrl->states[t.toState];
                ImRect rD = NodeRect(t.toState, d, origin);
                ImVec2 p2 = PinPosIn(rD);

                ImVec2 c1 = VAdd(p1, ImVec2(60.0f, 0));
                ImVec2 c2 = VSub(p2, ImVec2(60.0f, 0));

                bool selected = (m_SelectedLink.src == (int)i && m_SelectedLink.dst == (int)t.toState);
                dl->AddBezierCubic(p1, c1, c2, p2, selected ? colSel : col, thickness);

                float dist = 0.0f;
                if (LinkHitTest(p1, p2, ImGui::GetIO().MousePos, thickness + 3.0f, dist)) {
                    if (dist < bestDist) { bestDist = dist; bestSrc = (int)i; bestDst = (int)t.toState; }
                }
            }
        }

        if (bestSrc >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_SelectedState = -1;
            m_SelectedLink = { bestSrc, bestDst };
        }

        if (m_SelectedLink.src >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            State& s = m_Ctrl->states[(uint32_t)m_SelectedLink.src];
            s.transitions.erase(std::remove_if(s.transitions.begin(), s.transitions.end(),
                [&](const Transition& t) { return (int)t.toState == m_SelectedLink.dst; }), s.transitions.end());
            m_SelectedLink = {};
        }
    }

    bool AnimatorGraphPanel::LinkHitTest(const ImVec2& p1, const ImVec2& p2, const ImVec2& mouse, float thickness, float& distOut) {
        ImVec2 c1 = VAdd(p1, ImVec2(60, 0));
        ImVec2 c2 = VSub(p2, ImVec2(60, 0));
        const int steps = 20;
        float best = std::numeric_limits<float>::max();
        ImVec2 prev = p1;
        for (int i = 1; i <= steps; ++i) {
            float t = (float)i / steps;
            ImVec2 a = VLerp(p1, c1, t);
            ImVec2 b = VLerp(c1, c2, t);
            ImVec2 c = VLerp(c2, p2, t);
            ImVec2 d = VLerp(a, b, t);
            ImVec2 e = VLerp(b, c, t);
            ImVec2 q = VLerp(d, e, t);
            ImVec2 v = ImVec2(q.x - prev.x, q.y - prev.y);
            ImVec2 w = ImVec2(mouse.x - prev.x, mouse.y - prev.y);
            float c2v = v.x * v.x + v.y * v.y;
            float tproj = c2v > 0 ? (w.x * v.x + w.y * v.y) / c2v : 0.0f;
            tproj = std::clamp(tproj, 0.0f, 1.0f);
            ImVec2 proj = ImVec2(prev.x + tproj * v.x, prev.y + tproj * v.y);
            float dx = proj.x - mouse.x, dy = proj.y - mouse.y;
            best = std::min(best, std::sqrt(dx * dx + dy * dy));
            prev = q;
        }
        distOut = best;
        return best <= thickness + 4.0f;
    }

    void AnimatorGraphPanel::RightClickContext(const ImVec2& canvasMouse, const ImVec2& origin) {
        ImGui::OpenPopup("ctx");
        if (ImGui::BeginPopup("ctx")) {
            if (ImGui::MenuItem("Add State")) {
                State st; st.name = "New State"; st.clipId = ""; st.speed = 1.0f;
                m_Ctrl->states.push_back(st);
                uint32_t idx = (uint32_t)m_Ctrl->states.size() - 1;
                ImVec2 rel = VSub(canvasMouse, origin);
                m_NodePos[idx] = ImVec2(rel.x / m_Zoom, rel.y / m_Zoom);
            }
            if (m_SelectedState >= 0 && ImGui::MenuItem("Delete State")) {
                const int del = m_SelectedState;
                for (auto& s : m_Ctrl->states) {
                    s.transitions.erase(std::remove_if(s.transitions.begin(), s.transitions.end(),
                        [&](const Transition& t) { return (int)t.toState == del; }), s.transitions.end());
                    for (auto& t : s.transitions) if ((int)t.toState > del) t.toState--;
                }
                m_Ctrl->states.erase(m_Ctrl->states.begin() + del);
                std::unordered_map<uint32_t, ImVec2> np;
                for (auto& kv : m_NodePos) {
                    uint32_t k = kv.first;
                    if ((int)k == del) continue;
                    if ((int)k > del) k--;
                    np[k] = kv.second;
                }
                m_NodePos.swap(np);
                m_SelectedState = -1; m_SelectedLink = {};
            }
            ImGui::EndPopup();
        }
    }

    void AnimatorGraphPanel::DrawInspector() {
        if (!m_Ctrl) { ImGui::TextDisabled("No controller loaded."); return; }

        // --- STATE PROPERTIES ---
        if (m_SelectedState >= 0 && m_SelectedState < (int)m_Ctrl->states.size()) {
            State& s = m_Ctrl->states[(uint32_t)m_SelectedState];
            ImGui::TextUnformatted("State");
            ImGui::Separator();

            char nameBuf[128]; std::snprintf(nameBuf, sizeof(nameBuf), "%s", s.name.c_str());
            if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf))) s.name = nameBuf;

            char clipBuf[256]; std::snprintf(clipBuf, sizeof(clipBuf), "%s", s.clipId.c_str());
            if (ImGui::InputText("Clip", clipBuf, IM_ARRAYSIZE(clipBuf))) s.clipId = clipBuf;

            ImGui::DragFloat("Local Speed", &s.speed, 0.01f, 0.0f, 4.0f);
            return;
        }

        // --- TRANSITION PROPERTIES ---
        if (m_SelectedLink.src >= 0) {
            State& src = m_Ctrl->states[(uint32_t)m_SelectedLink.src];
            auto it = std::find_if(src.transitions.begin(), src.transitions.end(),
                [&](const Transition& t) { return (int)t.toState == m_SelectedLink.dst; });

            if (it != src.transitions.end()) {
                Transition& T = *it;

                ImGui::Text("Transition %s -> %s",
                    m_Ctrl->states[(uint32_t)m_SelectedLink.src].name.c_str(),
                    m_Ctrl->states[(uint32_t)T.toState].name.c_str());
                ImGui::Separator();

                ImGui::Checkbox("Has Exit Time", &T.hasExitTime);
                ImGui::DragFloat("Exit Time (norm)", &T.exitTimeNormalized, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Duration", &T.duration, 0.01f, 0.0f, 2.0f);
                ImGui::Checkbox("Allow Self", &T.canTransitionToSelf);

                ImGui::Separator();
                ImGui::TextUnformatted("Conditions");
                if (ImGui::Button("Add Condition")) {
                    Condition c;
                    c.param = (m_Ctrl->parameters.empty() ? "" : m_Ctrl->parameters[0].name);
                    c.op = CondOp::Greater;
                    c.f = 0.1f;
                    T.conditions.push_back(c);
                }

                using NE::Animation::ParamType;
                using NE::Animation::Parameter;

                // helper: find a parameter by name
                auto findParam = [&](const std::string& name) -> const Parameter* {
                    for (const auto& p : m_Ctrl->parameters) if (p.name == name) return &p;
                    return nullptr;
                    };

                for (size_t i = 0; i < T.conditions.size(); ++i) {
                    Condition& C = T.conditions[i];
                    ImGui::PushID((int)i);

                    // Param combo
                    const char* currentParam = C.param.empty() ? "(none)" : C.param.c_str();
                    if (ImGui::BeginCombo("Param", currentParam)) {
                        for (const auto& p : m_Ctrl->parameters) {
                            bool sel = (p.name == C.param);
                            if (ImGui::Selectable(p.name.c_str(), sel)) C.param = p.name;
                            if (sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    // Determine param type (defaults to Float-style if missing)
                    const Parameter* P = findParam(C.param);
                    ParamType ptype = P ? P->type : ParamType::Float;

                    // Build allowed operators for this param type
                    struct OpItem { const char* label; CondOp op; };
                    static const OpItem floatIntOps[] = {
                        { ">",  CondOp::Greater },
                        { "<",  CondOp::Less    },
                        { "==", CondOp::Equals  },
                        { "!=", CondOp::NotEquals }
                    };
                    static const OpItem boolTrigOps[] = {
                        { "If",    CondOp::If    },
                        { "IfNot", CondOp::IfNot }
                    };

                    const OpItem* allowed = nullptr;
                    int allowedCount = 0;
                    if (ptype == ParamType::Float || ptype == ParamType::Int) {
                        allowed = floatIntOps; allowedCount = (int)(sizeof(floatIntOps) / sizeof(floatIntOps[0]));
                        // coerce invalid op to a sensible default
                        if (C.op == CondOp::If || C.op == CondOp::IfNot) C.op = CondOp::Greater;
                    }
                    else { // Bool or Trigger
                        allowed = boolTrigOps; allowedCount = (int)(sizeof(boolTrigOps) / sizeof(boolTrigOps[0]));
                        if (!(C.op == CondOp::If || C.op == CondOp::IfNot)) C.op = CondOp::If;
                    }

                    // Current op label
                    const char* currOpLabel = allowed[0].label;
                    for (int k = 0; k < allowedCount; ++k)
                        if (allowed[k].op == C.op) { currOpLabel = allowed[k].label; break; }

                    // Op combo (only valid ops)
                    if (ImGui::BeginCombo("Op", currOpLabel)) {
                        for (int k = 0; k < allowedCount; ++k) {
                            bool sel = (allowed[k].op == C.op);
                            if (ImGui::Selectable(allowed[k].label, sel)) C.op = allowed[k].op;
                            if (sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    // Value field: only the one relevant to the Param type
                    switch (ptype) {
                    case ParamType::Float:
                        ImGui::DragFloat("Value", &C.f, 0.01f);
                        break;
                    case ParamType::Int:
                    {
                        int iv = C.i;
                        if (ImGui::DragInt("Value", &iv, 1)) C.i = iv;
                        break;
                    }
                    case ParamType::Bool:
                        ImGui::Checkbox("Value", &C.b);
                        break;
                    case ParamType::Trigger:
                        ImGui::TextDisabled("Triggers use If / IfNot only (no value).");
                        break;
                    }

                    // Remove condition
                    if (ImGui::Button("Remove")) {
                        T.conditions.erase(T.conditions.begin() + i);
                        ImGui::PopID();
                        break; // list changed; restart loop safely next frame
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }

                return;
            }
        }

        ImGui::TextDisabled("Select a state or a transition.");
    }

} // namespace Editor
