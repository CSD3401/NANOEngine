#ifndef AUDIOBANK_HPP
#define AUDIOBANK_HPP

// This is a FMOD Studio bank asset that can be loaded from a .bank file
// usually for a .bank file there will be eg master.bank and master.string.bank
// master.bank is the main bank file while master.string.bank acts like a 
// decryption to make the audionames human readable eg "footsteps" rather than
// "skadf23ro" etc

#include <string>
#include <fmod/fmod_studio.hpp>
#include "../Asset.hpp"
#include <unordered_map>


#pragma warning (push)
#pragma warning (disable : 4251)

namespace NE::Asset
{
	class NANOENGINE_API AudioBank : public IAsset
	{
	public:

		struct EventInfo 
		{
			std::string path;        // "event:/Footsteps/Concrete"
			std::string audioName; // "Footsteps Concrete"
			FMOD::Studio::EventDescription* eventDesc = nullptr;
		};

		AudioBank();
		virtual ~AudioBank();

		// return true if loaded succesfully
		virtual bool LoadFromFile(const std::string& filename) override;
		
		FMOD::Studio::Bank* GetFMODBank() const { return m_bank; }

		bool IsLoaded() const { return m_bank != nullptr; }

		// Unload bank and release FMOD resources
		void Unload();

		// Get display name of the bank w/o file extension
		std::string GetDisplayName() const;

		void ExtractEvents(FMOD::Studio::System* studioSystem);
		const std::unordered_map<std::string, EventInfo>& GetEvents() const { return m_events; }
		bool hasEvent(const std::string& eventPath) const;
		void SetFMODBank(FMOD::Studio::Bank* bank) { m_bank = bank; }

	private:
		FMOD::Studio::Bank* m_bank;
		bool m_loaded;
		std::unordered_map<std::string, EventInfo> m_events;

	};
}
#pragma warning(pop)

#endif // !AUDIOBANK_HPP
