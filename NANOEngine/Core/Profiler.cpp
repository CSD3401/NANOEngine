#include "Profiler.hpp"

#include <stack>

namespace {
    struct EventEntry {
        const char* name;
        Profiler::Clock::time_point start;
        uint8_t depth;
    };

    std::vector<ProfileEvent> s_currentEvents;
    std::vector<ProfileEvent> s_frameData;
    std::vector<EventEntry> s_stack;
    Profiler::Clock::time_point s_frameStart;
}

void Profiler::BeginFrame() {
    s_currentEvents.clear();
    s_stack.clear();
    s_frameStart = Clock::now();
}

void Profiler::EndFrame() {
    s_frameData = s_currentEvents;
}

void Profiler::PushEvent(const char* name) {
    EventEntry entry{ name, Clock::now(), static_cast<uint8_t>(s_stack.size()) };
    s_stack.push_back(entry);
}

void Profiler::PopEvent() {
    auto end = Clock::now();
    if (s_stack.empty())
        return;
    EventEntry entry = s_stack.back();
    s_stack.pop_back();

    ProfileEvent evt;
    evt.Name = entry.name;
    evt.Start = std::chrono::duration<double>(entry.start - s_frameStart).count();
    evt.End = std::chrono::duration<double>(end - s_frameStart).count();
    evt.Depth = entry.depth;
    s_currentEvents.push_back(evt);
}

const std::vector<ProfileEvent>& Profiler::GetFrameData() {
    return s_frameData;
}