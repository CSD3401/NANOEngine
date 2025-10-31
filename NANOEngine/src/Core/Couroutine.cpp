#include "Couroutine.hpp"

static CoroutineManager g_CoroutineManager;

CoroutineHandle CoroutineManager::Start(void* userData, bool (*updateFunc)(void*, float))
{
    if (!updateFunc) return 0;
    unsigned int id = m_NextHandle++;
    m_Entries[id] = { id, userData, updateFunc };
    return id;
}

void CoroutineManager::Stop(CoroutineHandle handle)
{
    m_Entries.erase(handle);
}

bool CoroutineManager::IsRunning(CoroutineHandle handle) const
{
    return m_Entries.find(handle) != m_Entries.end();
}

void CoroutineManager::Update(float dt)
{
    std::vector<unsigned int> toRemove;

    for (auto& [id, entry] : m_Entries)
    {
        bool finished = false;
        try {
            finished = entry.updateFunc(entry.userData, dt);
        }
        catch (...) {
            finished = true; // error -> terminate
        }

        if (finished)
            toRemove.push_back(id);
    }

    for (auto id : toRemove)
        m_Entries.erase(id);
}

CoroutineHandle Engine_StartCoroutine(CoroutineUpdateFunc updateFunc, void* userData)
{
    return g_CoroutineManager.Start(userData, updateFunc);
}

void Engine_StopCoroutine(CoroutineHandle handle)
{
    g_CoroutineManager.Stop(handle);
}

bool Engine_IsCoroutineRunning(CoroutineHandle handle)
{
    return g_CoroutineManager.IsRunning(handle);
}

// Engine-internal call per frame
void Engine_UpdateCoroutines(float dt)
{
    g_CoroutineManager.Update(dt);
}

CoroutineHandle Engine_WaitForSeconds(float seconds)
{
    auto* s = new CoroutineWaitForSeconds{ seconds };
    return Engine_StartCoroutine(&CoroutineWaitForSeconds::Update, s);
}