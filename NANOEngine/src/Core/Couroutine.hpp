#pragma once
#include <vector>
#include <functional>
#include <memory>
#include "NANOEngineAPI.hpp"

#pragma warning(push)
#pragma warning(disable: 4251)

// Handle type
typedef unsigned int CoroutineHandle;

struct CoroutineStep
{
    enum class Type
    {
        Action,
        WaitSeconds,
    };

    Type type = Type::Action;
    float waitTime = 0.0f;
    std::shared_ptr<std::function<void()>> action;
};

struct Coroutine
{
    std::vector<CoroutineStep> steps;
    size_t currentStepIndex = 0;
    float currentWait = 0.0f;
    bool finished = false;
};

class NANOENGINE_API CoroutineManager
{
public:
    CoroutineHandle CreateCoroutine();
    void AddAction(CoroutineHandle handle, std::function<void()> action);
    void AddWait(CoroutineHandle handle, float seconds);
    void Start(CoroutineHandle handle);
    void Update(float dt);

    void Clear();  // Clear all coroutines
    void StopCoroutine(CoroutineHandle handle);  // Stop a specific coroutine
    bool IsRunning(CoroutineHandle handle) const;  // Check if coroutine is still running

private:
    std::vector<Coroutine> m_coroutines;  // Direct vector of Coroutines
};

NANOENGINE_API CoroutineHandle Engine_CreateCoroutine();
NANOENGINE_API void Engine_AddAction(CoroutineHandle handle, void (*func)());
NANOENGINE_API void Engine_AddWaitForSeconds(CoroutineHandle handle, float seconds);
NANOENGINE_API void Engine_StartCoroutine(CoroutineHandle handle);
NANOENGINE_API void Engine_UpdateCoroutines(float deltaTime);

// C++ API for lambdas and any callable
NANOENGINE_API void Engine_AddActionCpp(CoroutineHandle handle, std::function<void()> func);

NANOENGINE_API void Engine_ClearAllCoroutines();
NANOENGINE_API void Engine_StopCoroutine(CoroutineHandle handle);
NANOENGINE_API bool Engine_IsCoroutineRunning(CoroutineHandle handle);

// Convenience overload that forwards to the C++ version
template<typename Func>
inline void Engine_AddAction(CoroutineHandle handle, Func&& func)
{
    Engine_AddActionCpp(handle, std::function<void()>(std::forward<Func>(func)));
}

#pragma warning(pop)