#ifndef TIMER_HPP
#define TIMER_HPP

#include "../NANOEngineAPI.hpp"
#include <chrono>
#include <cassert>

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

class NANOENGINE_API Timer {
public:
    using Clock = std::chrono::steady_clock; // Monotonic clock
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double>;

    Timer(double fixedDeltaTime = 1.0 / 60.0, int maxSteps = 5);
    void Start();
    void Update();

    inline TimePoint GetStartTime() const { return startTime; }
    inline TimePoint GetCurrentTime() const { return currentTime; }
    inline double GetDeltaTime() const { return deltaTime; }
    inline double GetFixedDeltaTime() const { return fixedDeltaTime; }
    inline double GetElapsedTime() const { return elapsedTime; }
    inline int GetFPS() const { return fps; }
    inline int GetNumOfSteps() const { return numOfSteps; }

private:
    // Helper to convert a duration to seconds.
    static double DurationInSeconds(const Duration& duration) { return duration.count(); }

    // Computes the number of fixed-update steps given the delta time.
    void CalculateNumOfSteps(double dt);

    // Timing variables:
    TimePoint startTime;        // Overall start time of the timer.
    TimePoint lastFPSTime;      // Time at which the last FPS update occurred.
    TimePoint previousTime;     // Time at the previous Update() call.
    TimePoint currentTime;      // Current time from the last Update() call.

    double elapsedTime = 0.0;   // Total time since Start() was called.
    double deltaTime = 0.0;     // Time between the current and previous frame.
    int fps = 0;                // Frames per second.
    int frameCounter = 0;       // Counts frames in the current FPS interval.

    double fixedDeltaTime;      // Configurable fixed time step for fixed updates.
    double dtAccumulator = 0.0; // Accumulates delta time for fixed-update steps.
    int numOfSteps = 0;         // The number of fixed update steps computed for this frame.
    int maxSteps;               // Maximum allowed fixed update steps per frame.
};

#endif // !TIMER_HPP