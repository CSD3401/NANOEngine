#include "AudioBank.hpp"
#include <filesystem>
#include "Core/SpdLogger.hpp"

namespace NE::Asset
{
	AudioBank::AudioBank()
		: m_bank(nullptr)
		, m_loaded(false)
	{

	}

	AudioBank::~AudioBank()
	{
		Unload();
	}

	/*
	Actual FMOD bank loading happens in AudioSystem
	This class jus tracks AudioBank asset metadata
	FMOD bank pointer will be set by AudioSystem after loading
	*/
	bool AudioBank::LoadFromFile(const std::string& filename)
	{
		filePath = filename;
		displayName = std::filesystem::path(filename).stem().string();
		m_loaded = true;
		return true;
	}


	/*
	FMOD bank release should be handled by AudioSystem
	This just resets reference
	*/
	void AudioBank::Unload()
	{
		if (m_bank) 
		{
			m_bank->unload();
			m_bank = nullptr;
		}
		m_loaded = false;
		m_events.clear();
	}

	std::string AudioBank::GetDisplayName() const
	{
		return displayName.empty() ? std::filesystem::path(filePath).stem().string() : displayName;
	}
	void AudioBank::ExtractEvents(FMOD::Studio::System* /*studioSystem*/)
	{
		m_events.clear();

		if (!m_bank) 
		{
			return;
		}

		// Get event count in this bank
		int eventCount = 0;
		m_bank->getEventCount(&eventCount);

		if (eventCount > 0) {
			// Get all events in this bank
			std::vector<FMOD::Studio::EventDescription*> events(eventCount);
			m_bank->getEventList(events.data(), eventCount, &eventCount);

			for (int i = 0; i < eventCount; ++i) {
				FMOD::Studio::EventDescription* eventDesc = events[i];

				// Get event path
				char eventPath[256];
				int retrieved = 0;
				eventDesc->getPath(eventPath, sizeof(eventPath), &retrieved);

				if (retrieved > 0) 
				{
					std::string pathStr = eventPath;

					std::string displayedName;
					if (pathStr.find("event:/") == 0 && pathStr.length() > 7) {
						displayedName = pathStr.substr(7); // Remove "event:/" (7 characters)
					}
					else {
						displayedName = pathStr; // Use full path if it doesn't match expected format
					}

					// Replace slashes with spaces for readability
					std::replace(displayedName.begin(), displayedName.end(), '/', ' ');

					// Store event info
					EventInfo eventInfo;
					eventInfo.path = eventPath;
					//eventInfo.displayName = displayedName;
					eventInfo.eventDesc = eventDesc;

					m_events[eventPath] = eventInfo;

					SPD_INFO("Found event: " << eventPath << " -> " << displayedName);
				}
			}
		}
	}

	bool AudioBank::hasEvent(const std::string& eventPath) const
	{
		return m_events.find(eventPath) != m_events.end();
	}
}