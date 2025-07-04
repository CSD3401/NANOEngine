#include "Timer.hpp"

Timer::Timer(double fixedDeltaTime, int maxSteps)
    : fixedDeltaTime(fixedDeltaTime), maxSteps(maxSteps) {
}

void Timer::Start() {
    startTime = Clock::now();
    lastFPSTime = startTime;
    previousTime = startTime;
}

void Timer::Update() {
    currentTime = Clock::now();

    // Update total elapsed time and calculate delta time.
    elapsedTime = DurationInSeconds(currentTime - startTime);
    deltaTime = DurationInSeconds(currentTime - previousTime);
    previousTime = currentTime;

    // Calculate FPS using a rolling interval.
    frameCounter++;
    double interval = DurationInSeconds(currentTime - lastFPSTime);
    if (interval >= 1.0) {
        fps = static_cast<int>(frameCounter / interval);
        frameCounter = 0;
        lastFPSTime = currentTime;
    }

    // Determine the number of fixed update steps.
    CalculateNumOfSteps(deltaTime);
}

void Timer::CalculateNumOfSteps(double dt) {
    numOfSteps = 0;
    dtAccumulator += dt;

    // Prevent spiral of death by capping the number of fixed update steps.
    while (dtAccumulator >= fixedDeltaTime && numOfSteps < maxSteps) {
        dtAccumulator -= fixedDeltaTime;
        numOfSteps++;
    }
}