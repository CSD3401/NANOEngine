#ifndef PROFILER_HPP
#define PROFILER_HPP

#include "../NANOEngineAPI.hpp"
#include <chrono>
#include <cstdint>
#include <vector>

struct NANOENGINE_API ProfileEvent {
    const char* Name;
    double Start;
    double End;
    uint8_t Depth;
};

class NANOENGINE_API Profiler {
public:
    using Clock = std::chrono::steady_clock;

    static void BeginFrame();
    static void EndFrame();
    static void PushEvent(const char* name);
    static void PopEvent();
    static const std::vector<ProfileEvent>& GetFrameData();

    class Scope {
    public:
        explicit Scope(const char* name) { Profiler::PushEvent(name); }
        ~Scope() { Profiler::PopEvent(); }
    };
};

#define NE_CONCAT_INTERNAL(x, y) x##y
#define NE_CONCAT(x, y) NE_CONCAT_INTERNAL(x, y)
#define NE_PROFILE_SCOPE(name) Profiler::Scope NE_CONCAT(_ne_scope_, __LINE__)(name)
#define NE_PROFILE_FUNCTION() NE_PROFILE_SCOPE(__FUNCTION__)

#endif // PROFILER_HPP
