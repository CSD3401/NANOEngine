#include "ProfilerPanel.hpp"

#include <imgui/imgui.h>
#include <imgui/widgets/imgui_widget_flamegraph/imgui_widget_flamegraph.h>
#include <Core/Profiler.hpp>
#include "../Application.hpp"
#include <Engine.hpp>

namespace Editor {

    static void FlameGetter(float* start, float* end, ImU8* level, const char** caption, const void* data, int idx)
    {
        const auto* events = static_cast<const std::vector<ProfileEvent>*>(data);
        const auto& ev = (*events)[idx];
        if (start) *start = static_cast<float>(ev.Start);
        if (end) *end = static_cast<float>(ev.End);
        if (level) *level = ev.Depth;
        if (caption) *caption = ev.Name;
    }

    void ProfilerPanel::OnImGuiRender()
    {
        ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const auto& data = Profiler::GetFrameData();
        ImGuiWidgetFlameGraph::PlotFlame("Flame", FlameGetter, &data, static_cast<int>(data.size()));

        if (ImGui::BeginTable("##profilerevents", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Start (s)");
            ImGui::TableSetupColumn("End (s)");
            ImGui::TableSetupColumn("Duration (s)");
            ImGui::TableHeadersRow();

            for (const auto& ev : data) {
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(ev.Name);

                ImGui::TableNextColumn();
                ImGui::Text("%.4f", ev.Start);

                ImGui::TableNextColumn();
                ImGui::Text("%.4f", ev.End);

                ImGui::TableNextColumn();
                ImGui::Text("%.4f", ev.End - ev.Start);
            }

            ImGui::EndTable();
        }

        ImGui::Text("FPS: %d", Application::timer.GetFPS());

        ImGui::Text("Scene Draw Call Count: %d", NE::GetSceneDrawCallCount());
        ImGui::Text("Game Draw Call Count: %d", NE::GetGameDrawCallCount());

        ImGui::End();
    }

}