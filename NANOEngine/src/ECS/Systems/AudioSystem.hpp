#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/AudioSource.hpp"
#include "../../../NANOEngine/ThirdParty/include/fmod/fmod.hpp"


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

	private:
		ComponentManager* m_componentManager;
		void help();

		NE::ECS::Component::AudioSource x;

		void ProcessAudioSource(
			NE::ECS::Component::AudioSource& source,
			NE::ECS::Component::AudioClip& clip,
			FMOD::System* system);

		void PlayAudio(
			NE::ECS::Component::AudioSource& source,
			NE::ECS::Component::AudioClip& clip,
			FMOD::System* system);

		void UpdateAudioPlayback(
			NE::ECS::Component::AudioSource& source,
			NE::ECS::Component::AudioClip& clip,
			FMOD::System* system);
	};
}