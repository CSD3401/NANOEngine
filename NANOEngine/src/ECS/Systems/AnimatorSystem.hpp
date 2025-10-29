#pragma once
#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/Animator.hpp"
#include "../Components/Transform.hpp"
#include "../../Core/SpdLogger.hpp"
#include <unordered_map>
#include <memory>
#include "../../Animation/TransformClip.hpp"

namespace NE::ECS::Systems {

    class AnimatorSystem final : public System {
    public:
        explicit AnimatorSystem(ComponentManager* cm) : m_componentManager(cm) {}

        void OnEntityAdded(Entity) override {}
        void OnEntityRemoved(Entity) override {}

        void Init() override {}
        void Exit() override {}

        void Update(double deltaTime) override;

        // TEMP: simple in-memory clip registry
        void RegisterClip(const std::string& id, std::shared_ptr<NE::Animation::TransformClip> clip) {
            m_clips[id] = std::move(clip);
        }
        std::shared_ptr<NE::Animation::TransformClip> GetClip(const std::string& id) const {
            auto it = m_clips.find(id);
            return (it == m_clips.end()) ? nullptr : it->second;
        }

    private:
        ComponentManager* m_componentManager;
        std::unordered_map<std::string, std::shared_ptr<NE::Animation::TransformClip>> m_clips;
    };

} // namespace NE::ECS::Systems
