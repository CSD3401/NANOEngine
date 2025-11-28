#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/AudioSource.hpp"
#include "../Components/Transform.hpp"
#include <fmod/fmod.hpp>
#include <fmod/fmod_studio.hpp>
//#include "../../AssetManager.hpp"
#include "../../Audio/AudioBank.hpp"
#include <map>

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
		void StopSound(const std::string& eventName);
		void CleanupStudioSystem();
		void LoadBankAssets(const std::string& audioDirectory);

		struct AudioEvent
		{
			std::string path;        // "event:/Footsteps/Concrete"
			std::string displayName; // "Footsteps Concrete"
			std::string bankName;    // "Master.bank"
		};

		struct BankData
		{
			std::string filepath;
			FMOD::Studio::Bank* bank;
		};

		//const std::vector<std::pair<std::string, std::shared_ptr<NE::Asset::AudioBank>>>& GetLoadedBanks() const;
		std::unordered_map<std::string, NE::Asset::AudioBank::EventInfo> GetAllEvents() const;


	private:

		ComponentManager* m_componentManager;

		// Following function are deprecated do not use
		// Generally non-Raphael should only be using PlaySound()

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