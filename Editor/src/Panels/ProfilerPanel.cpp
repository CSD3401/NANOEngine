#include "ProfilerPanel.hpp"

#include <imgui/imgui.h>
#include <imgui/widgets/imgui_widget_flamegraph/imgui_widget_flamegraph.h>
#include <Core/Profiler.hpp>

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
        ImGui::End();
    }

}