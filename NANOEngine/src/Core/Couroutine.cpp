#include "Couroutine.hpp"

static CoroutineManager g_CoroutineManager;

CoroutineHandle CoroutineManager::CreateCoroutine()
{
    m_coroutines.emplace_back();
    return static_cast<CoroutineHandle>(m_coroutines.size() - 1);
}

void CoroutineManager::AddAction(CoroutineHandle handle, std::function<void()> action)
{
    if (handle >= m_coroutines.size()) return;
    CoroutineStep step;
    step.type = CoroutineStep::Type::Action;
    step.action = std::make_shared<std::function<void()>>(std::move(action));
    m_coroutines[handle].steps.push_back(std::move(step));
}

void CoroutineManager::AddWait(CoroutineHandle handle, float seconds)
{
    if (handle >= m_coroutines.size()) return;
    CoroutineStep step;
    step.type = CoroutineStep::Type::WaitSeconds;
    step.waitTime = seconds;
    m_coroutines[handle].steps.push_back(step);
}

void CoroutineManager::Start(CoroutineHandle handle)
{
    // No-op for now, coroutines begin automatically during update
}

void CoroutineManager::Update(float dt)
{
    for (auto& c : m_coroutines)
    {
        if (c.finished) continue;

        if (c.currentWait > 0.0f)
        {
            c.currentWait -= dt;
            continue;
        }

        if (c.currentStepIndex >= c.steps.size())
        {
            c.finished = true;
            continue;
        }

        auto& step = c.steps[c.currentStepIndex];

        switch (step.type)
        {
        case CoroutineStep::Type::Action:
            if (step.action && *step.action)
                (*step.action)();
            c.currentStepIndex++;
            break;

        case CoroutineStep::Type::WaitSeconds:
            c.currentWait = step.waitTime;
            c.currentStepIndex++;
            break;
        }
    }
}

NANOENGINE_API CoroutineHandle Engine_CreateCoroutine()
{
    return g_CoroutineManager.CreateCoroutine();
}

NANOENGINE_API void Engine_AddWaitForSeconds(CoroutineHandle handle, float seconds)
{
    g_CoroutineManager.AddWait(handle, seconds);
}

NANOENGINE_API void Engine_StartCoroutine(CoroutineHandle handle)
{
    g_CoroutineManager.Start(handle);
}

NANOENGINE_API void Engine_UpdateCoroutines(float deltaTime)
{
    g_CoroutineManager.Update(deltaTime);
}

NANOENGINE_API void Engine_AddAction(CoroutineHandle handle, void (*func)())
{
    g_CoroutineManager.AddAction(handle, [func]() { func(); });
}

NANOENGINE_API void Engine_AddActionCpp(CoroutineHandle handle, std::function<void()> func)
{
    g_CoroutineManager.AddAction(handle, std::move(func));
}
