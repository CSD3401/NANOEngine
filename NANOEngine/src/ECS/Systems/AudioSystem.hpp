#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/AudioSource.hpp"
#include "../Components/Transform.hpp"
#include "../../../NANOEngine/ThirdParty/include/fmod/fmod.hpp"
#include "../../../NANOEngine/ThirdParty/include/fmod/fmod_errors.h"
#include "../../../NANOEngine/ThirdParty/include/fmod/fmod_studio.hpp"


namespace NE::ECS::Systems 
{
	class AudioSystem final : public System {
	public:
		explicit AudioSystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;
		
		void Init() override;
		void Update(double deltaTime) override; // override in concrete systems
		void Exit() override;

		// Fmod Studio linking test code
		FMOD::Studio::System* studioSystem = nullptr;
		void SetupStudioSystem();
		void PlaySound(const std::string& eventName);



	private:
		ComponentManager* m_componentManager;

		NE::ECS::Component::AudioSource x;

		// Following function should only have read-only transform

		void ProcessAudioSource(
			NE::ECS::Component::AudioSource& source,
			const NE::ECS::Component::Transform& transform,
			FMOD::System* system);

		void PlayAudio(
			NE::ECS::Component::AudioSource& source,
			const NE::ECS::Component::Transform& transform,
			FMOD::System* system);

		void UpdateAudioPlayback(
			NE::ECS::Component::AudioSource& source,
			const NE::ECS::Component::Transform& transform,
			FMOD::System* system);
	};
}