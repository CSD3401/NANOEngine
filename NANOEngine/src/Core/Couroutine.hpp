#pragma once
#include <unordered_map>
#include <functional>
#include "NANOEngineAPI.hpp"

#pragma warning(push)
#pragma warning(disable: 4251)

// Handle type
typedef unsigned int CoroutineHandle;

// Function type for coroutine update step
// return true -> coroutine finished
// return false -> keep running
typedef bool (*CoroutineUpdateFunc)(void* userData, float deltaTime);

// Start coroutine (engine will call updateFunc every frame until it returns true)
NANOENGINE_API CoroutineHandle Engine_StartCoroutine(CoroutineUpdateFunc updateFunc, void* userData);

// Stop manually
NANOENGINE_API void Engine_StopCoroutine(CoroutineHandle handle);

// Check if still running
NANOENGINE_API bool Engine_IsCoroutineRunning(CoroutineHandle handle);

NANOENGINE_API void Engine_UpdateCoroutines(float dt);

struct NANOENGINE_API CoroutineEntry
{
    unsigned int handle;
    void* userData;
    bool (*updateFunc)(void*, float);
};

class NANOENGINE_API CoroutineManager
{
public:
    CoroutineHandle Start(void* userData, bool (*updateFunc)(void*, float));
    void Stop(CoroutineHandle handle);
    bool IsRunning(CoroutineHandle handle) const;
    void Update(float dt); // call this every frame

private:
    std::unordered_map<CoroutineHandle, CoroutineEntry> m_Entries;
    unsigned int m_NextHandle = 1;
};

struct NANOENGINE_API CoroutineWaitForSeconds
{
    float timeLeft;
    static bool Update(void* data, float dt)
    {
        auto* s = static_cast<CoroutineWaitForSeconds*>(data);
        s->timeLeft -= dt;
        if (s->timeLeft <= 0.0f)
        {
            delete s;
            return true;
        }
        return false;
    }
};

NANOENGINE_API CoroutineHandle Engine_WaitForSeconds(float seconds);

#pragma warning(pop)
