#pragma once
#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/Animator.hpp"
#include "../Components/Transform.hpp"
#include "../../Core/SpdLogger.hpp"
#include <unordered_map>
#include <memory>
#include <filesystem>
#include "../../Animation/TransformClip.hpp"
#include "../../Animation/AnimatorController.hpp"

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
        std::unordered_map<std::string, std::shared_ptr<NE::Animation::AnimatorController>> m_controllers;
        std::unordered_map<Entity, NE::Animation::AnimatorInstance> m_instances;

        void RegisterController(const std::string& id, std::shared_ptr<NE::Animation::AnimatorController> ctrl) {
            m_controllers[id] = std::move(ctrl);
        }
        std::shared_ptr<NE::Animation::AnimatorController> LoadOrGetController(const std::string& path);
        std::shared_ptr<NE::Animation::TransformClip>     LoadOrGetClip(const std::string& id);
        std::unordered_map<std::string, std::filesystem::file_time_type> m_ctrlMTime;
        void EnsureControllerLoaded(const std::string& path);
    private:
        ComponentManager* m_componentManager;
        std::unordered_map<std::string, std::shared_ptr<NE::Animation::TransformClip>> m_clips;
    };

} // namespace NE::ECS::Systems
